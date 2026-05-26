#pragma once

#include "combolens/search/RouteSearch.hpp"

#include <sstream>

namespace combolens {

struct TransitionGraphNode {
    SearchState state;
    FrameState representative;
    int minDepth = 0;
};

struct TransitionGraphEdge {
    std::size_t from = 0;
    std::size_t to = 0;
    MoveId move = MOVE_NONE;
    int damage = 0;
    int difficulty = 0;
};

struct StateTransitionGraph {
    std::vector<TransitionGraphNode> nodes;
    std::vector<TransitionGraphEdge> edges;
    std::vector<std::vector<std::size_t>> outgoing;
};

struct DegenerateLoop {
    std::vector<std::size_t> nodes;
    std::vector<std::size_t> internalEdges;
    bool selfLoop = false;
};

struct DegenerateLoopReport {
    std::vector<DegenerateLoop> loops;

    bool hasDegenerateLoop() const {
        return !loops.empty();
    }
};

inline const char* searchSelfStateName(SearchSelfState s) {
    switch (s) {
        case SearchSelfState::Grounded: return "Grounded";
        case SearchSelfState::Airborne: return "Airborne";
    }
    return "?";
}

inline const char* searchOppStateName(SearchOppState s) {
    switch (s) {
        case SearchOppState::Grounded: return "Grounded";
        case SearchOppState::Airborne: return "Airborne";
        case SearchOppState::WallSplat: return "WallSplat";
        case SearchOppState::GroundBounce: return "GroundBounce";
        case SearchOppState::Knockdown: return "Knockdown";
        case SearchOppState::Neutral: return "Neutral";
    }
    return "?";
}

inline bool transitionGraphTerminal(const SearchState& s) {
    if (s.opp == SearchOppState::Knockdown) return true;
    return s.opp == SearchOppState::Neutral && s.lastMove != MOVE_NONE;
}

inline std::string searchStateLabel(const SearchState& s) {
    std::ostringstream os;
    os << searchSelfStateName(s.self) << "/" << searchOppStateName(s.opp)
       << "\\nact=" << static_cast<int>(s.selfActionableIn)
       << " hitstun=" << static_cast<int>(s.oppHitstunRemaining)
       << " cancel=" << static_cast<int>(s.cancelWindowRemaining)
       << "\\nh=" << static_cast<int>(s.height)
       << " d=" << static_cast<int>(s.distance)
       << " wall=" << static_cast<int>(s.wallDist)
       << "\\nm=" << static_cast<int>(s.meter)
       << " scale=" << static_cast<int>(s.scaling)
       << " juggle=" << static_cast<int>(s.juggle)
       << "\\nlast=" << s.lastMove;
    return os.str();
}

inline std::string moveDisplayName(const MoveDatabase& db, MoveId id) {
    if (id == MOVE_NONE) return "NONE";
    if (id == MOVE_END) return "END";
    return db.byId(id).name;
}

inline StateTransitionGraph buildStateTransitionGraph(
    const SearchNode& initial,
    const MoveDatabase& db,
    SearchSettings settings = {},
    SimSettings simSettings = {}
) {
    StateTransitionGraph graph;
    std::unordered_map<SearchState, std::size_t, SearchStateHash> nodeByState;
    std::vector<std::size_t> queue;

    auto addNode = [&](const SearchNode& node, int depth) {
        auto it = nodeByState.find(node.abstract);
        if (it != nodeByState.end()) {
            TransitionGraphNode& existing = graph.nodes[it->second];
            if (depth < existing.minDepth) {
                existing.minDepth = depth;
                existing.representative = node.representative;
            }
            return it->second;
        }

        const std::size_t id = graph.nodes.size();
        graph.nodes.push_back(TransitionGraphNode{node.abstract, node.representative, depth});
        graph.outgoing.emplace_back();
        nodeByState.emplace(node.abstract, id);
        queue.push_back(id);
        return id;
    };

    addNode(initial, 0);

    for (std::size_t cursor = 0; cursor < queue.size(); ++cursor) {
        const std::size_t from = queue[cursor];
        const TransitionGraphNode current = graph.nodes[from];
        if (current.minDepth >= settings.maxDepth) continue;
        if (transitionGraphTerminal(current.state)) continue;

        for (const MoveDef& move : db.moves()) {
            if (current.minDepth == 0 && settings.forcedStarter.has_value() && move.id != *settings.forcedStarter) {
                continue;
            }

            MoveDef candidate = move;
            if (current.minDepth == 0 && settings.forcedStarter.has_value() && move.id == *settings.forcedStarter) {
                candidate.canStartCombo = true;
            }

            SearchNode currentSearch{current.state, current.representative};
            TransitionResult tr = tryApplyMoveHybrid(currentSearch, candidate, db, simSettings);
            if (!tr.valid) continue;

            const std::size_t to = addNode(SearchNode{tr.next, tr.nextRepresentativeFrame}, current.minDepth + 1);
            const std::size_t edgeId = graph.edges.size();
            graph.edges.push_back(TransitionGraphEdge{from, to, move.id, tr.damage, tr.difficulty});
            graph.outgoing[from].push_back(edgeId);
        }
    }

    return graph;
}

inline DegenerateLoopReport detectDegenerateLoops(const StateTransitionGraph& graph) {
    struct TarjanState {
        int nextIndex = 0;
        std::vector<int> index;
        std::vector<int> lowlink;
        std::vector<std::size_t> stack;
        std::vector<bool> onStack;
        DegenerateLoopReport report;
    };

    TarjanState t;
    t.index.assign(graph.nodes.size(), -1);
    t.lowlink.assign(graph.nodes.size(), -1);
    t.onStack.assign(graph.nodes.size(), false);

    auto strongConnect = [&](auto&& self, std::size_t v) -> void {
        t.index[v] = t.nextIndex;
        t.lowlink[v] = t.nextIndex;
        ++t.nextIndex;
        t.stack.push_back(v);
        t.onStack[v] = true;

        for (std::size_t edgeId : graph.outgoing[v]) {
            const std::size_t w = graph.edges[edgeId].to;
            if (t.index[w] == -1) {
                self(self, w);
                t.lowlink[v] = std::min(t.lowlink[v], t.lowlink[w]);
            } else if (t.onStack[w]) {
                t.lowlink[v] = std::min(t.lowlink[v], t.index[w]);
            }
        }

        if (t.lowlink[v] != t.index[v]) return;

        DegenerateLoop loop;
        std::unordered_set<std::size_t> componentSet;
        while (true) {
            const std::size_t w = t.stack.back();
            t.stack.pop_back();
            t.onStack[w] = false;
            loop.nodes.push_back(w);
            componentSet.insert(w);
            if (w == v) break;
        }

        for (std::size_t nodeId : loop.nodes) {
            for (std::size_t edgeId : graph.outgoing[nodeId]) {
                const TransitionGraphEdge& edge = graph.edges[edgeId];
                if (componentSet.contains(edge.to)) {
                    loop.internalEdges.push_back(edgeId);
                    if (edge.from == edge.to) {
                        loop.selfLoop = true;
                    }
                }
            }
        }

        if (loop.nodes.size() > 1 || loop.selfLoop) {
            std::sort(loop.nodes.begin(), loop.nodes.end());
            std::sort(loop.internalEdges.begin(), loop.internalEdges.end());
            t.report.loops.push_back(loop);
        }
    };

    for (std::size_t i = 0; i < graph.nodes.size(); ++i) {
        if (t.index[i] == -1) {
            strongConnect(strongConnect, i);
        }
    }

    return t.report;
}

inline std::string transitionGraphToDot(
    const StateTransitionGraph& graph,
    const MoveDatabase& db,
    const DegenerateLoopReport* loopReport = nullptr
) {
    std::unordered_set<std::size_t> loopNodes;
    if (loopReport) {
        for (const DegenerateLoop& loop : loopReport->loops) {
            loopNodes.insert(loop.nodes.begin(), loop.nodes.end());
        }
    }

    std::ostringstream os;
    os << "digraph StateTransitionGraph {\n";
    os << "  rankdir=LR;\n";
    os << "  node [shape=box, style=\"rounded,filled\", fontname=\"Helvetica\"];\n";

    for (std::size_t i = 0; i < graph.nodes.size(); ++i) {
        const bool terminal = transitionGraphTerminal(graph.nodes[i].state);
        const bool inLoop = loopNodes.contains(i);
        const char* fill = inLoop ? "#ffd7d2" : (terminal ? "#d7eadf" : "#f7f5ef");
        os << "  s" << i << " [label=\"#" << i << " d" << graph.nodes[i].minDepth
           << "\\n" << searchStateLabel(graph.nodes[i].state)
           << "\", fillcolor=\"" << fill << "\"];\n";
    }

    for (const TransitionGraphEdge& edge : graph.edges) {
        os << "  s" << edge.from << " -> s" << edge.to
           << " [label=\"" << moveDisplayName(db, edge.move)
           << " dmg=" << edge.damage << "\"];\n";
    }

    os << "}\n";
    return os.str();
}

inline std::string transitionGraphSvgEscape(const std::string& text) {
    std::string out;
    for (char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c; break;
        }
    }
    return out;
}

inline void transitionGraphDrawSvgLine(
    std::ostringstream& os,
    Vec2 from,
    Vec2 to,
    const std::string& label,
    bool loopEdge
) {
    const char* color = loopEdge ? "#c7372f" : "#53626f";
    const float midX = (from.x + to.x) * 0.5f;
    const float midY = (from.y + to.y) * 0.5f;
    os << "<path marker-end=\"url(#arrow)\" d=\"M" << from.x + 82 << "," << from.y
       << " C" << midX << "," << from.y << " " << midX << "," << to.y
       << " " << to.x - 82 << "," << to.y << "\" fill=\"none\" stroke=\"" << color
       << "\" stroke-width=\"" << (loopEdge ? 2.2f : 1.6f) << "\"/>\n";
    os << "<rect x=\"" << midX - 34 << "\" y=\"" << midY - 11
       << "\" width=\"68\" height=\"20\" rx=\"4\" fill=\"#f7f5ef\" stroke=\"#cbc6b9\"/>\n";
    os << "<text class=\"edgeLabel\" text-anchor=\"middle\" x=\"" << midX
       << "\" y=\"" << midY + 4 << "\">" << transitionGraphSvgEscape(label) << "</text>\n";
}

inline std::string transitionGraphToSvg(
    const StateTransitionGraph& graph,
    const MoveDatabase& db,
    const DegenerateLoopReport* loopReport = nullptr
) {
    std::unordered_set<std::size_t> loopNodes;
    std::unordered_set<std::size_t> loopEdges;
    if (loopReport) {
        for (const DegenerateLoop& loop : loopReport->loops) {
            loopNodes.insert(loop.nodes.begin(), loop.nodes.end());
            loopEdges.insert(loop.internalEdges.begin(), loop.internalEdges.end());
        }
    }

    int maxDepth = 0;
    for (const TransitionGraphNode& node : graph.nodes) {
        maxDepth = std::max(maxDepth, node.minDepth);
    }

    std::vector<std::vector<std::size_t>> columns(static_cast<std::size_t>(maxDepth + 1));
    for (std::size_t i = 0; i < graph.nodes.size(); ++i) {
        columns[static_cast<std::size_t>(graph.nodes[i].minDepth)].push_back(i);
    }

    const int colWidth = 260;
    const int rowHeight = 150;
    int maxRows = 1;
    for (const auto& column : columns) {
        maxRows = std::max(maxRows, static_cast<int>(column.size()));
    }
    const int width = 140 + static_cast<int>(columns.size()) * colWidth;
    const int height = 170 + maxRows * rowHeight;

    std::unordered_map<std::size_t, Vec2> pos;
    for (std::size_t col = 0; col < columns.size(); ++col) {
        std::sort(columns[col].begin(), columns[col].end());
        for (std::size_t row = 0; row < columns[col].size(); ++row) {
            pos[columns[col][row]] = Vec2{
                90.0f + static_cast<float>(col) * static_cast<float>(colWidth),
                125.0f + static_cast<float>(row) * static_cast<float>(rowHeight)
            };
        }
    }

    std::ostringstream os;
    os << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width
       << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height << "\">\n";
    os << "<rect width=\"100%\" height=\"100%\" fill=\"#f7f5ef\"/>\n";
    os << "<style>text{font-family:Helvetica,Arial,sans-serif;font-size:12px}.title{font-size:18px;font-weight:700}.node{stroke:#2f302c;stroke-width:1.3}.edgeLabel{font-size:11px;fill:#333}.tiny{font-size:10px;fill:#555}</style>\n";
    os << "<defs><marker id=\"arrow\" markerWidth=\"10\" markerHeight=\"10\" refX=\"8\" refY=\"3\" orient=\"auto\" markerUnits=\"strokeWidth\"><path d=\"M0,0 L0,6 L9,3 z\" fill=\"#53626f\"/></marker></defs>\n";
    os << "<text class=\"title\" x=\"24\" y=\"32\">ComboLens State Transition Graph</text>\n";
    os << "<text x=\"24\" y=\"54\" fill=\"#555\">red nodes/edges are SCC degenerate loops; green nodes are terminal states</text>\n";

    for (std::size_t edgeId = 0; edgeId < graph.edges.size(); ++edgeId) {
        const TransitionGraphEdge& edge = graph.edges[edgeId];
        const std::string label = moveDisplayName(db, edge.move) + " dmg=" + std::to_string(edge.damage);
        transitionGraphDrawSvgLine(os, pos[edge.from], pos[edge.to], label, loopEdges.contains(edgeId));
    }

    for (std::size_t i = 0; i < graph.nodes.size(); ++i) {
        const Vec2 p = pos[i];
        const bool terminal = transitionGraphTerminal(graph.nodes[i].state);
        const bool inLoop = loopNodes.contains(i);
        const char* fill = inLoop ? "#ffd7d2" : (terminal ? "#d7eadf" : "#ffffff");
        os << "<rect class=\"node\" x=\"" << p.x - 82 << "\" y=\"" << p.y - 48
           << "\" width=\"164\" height=\"96\" rx=\"7\" fill=\"" << fill << "\"/>\n";
        os << "<text text-anchor=\"middle\" x=\"" << p.x << "\" y=\"" << p.y - 28 << "\">#"
           << i << " depth " << graph.nodes[i].minDepth << "</text>\n";

        const SearchState& state = graph.nodes[i].state;
        os << "<text class=\"tiny\" text-anchor=\"middle\" x=\"" << p.x << "\" y=\"" << p.y - 10
           << "\">" << searchSelfStateName(state.self) << " / " << searchOppStateName(state.opp) << "</text>\n";
        os << "<text class=\"tiny\" text-anchor=\"middle\" x=\"" << p.x << "\" y=\"" << p.y + 6
           << "\">act " << static_cast<int>(state.selfActionableIn)
           << " hitstun " << static_cast<int>(state.oppHitstunRemaining)
           << " cancel " << static_cast<int>(state.cancelWindowRemaining) << "</text>\n";
        os << "<text class=\"tiny\" text-anchor=\"middle\" x=\"" << p.x << "\" y=\"" << p.y + 22
           << "\">m " << static_cast<int>(state.meter)
           << " scale " << static_cast<int>(state.scaling)
           << " juggle " << static_cast<int>(state.juggle)
           << " last " << state.lastMove << "</text>\n";
    }

    os << "</svg>\n";
    return os.str();
}

} // namespace combolens
