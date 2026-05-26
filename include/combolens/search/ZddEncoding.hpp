#pragma once

#include "combolens/search/Route.hpp"

#include <sstream>

namespace combolens {

using ZddVar = std::uint32_t;
using ZddNodeId = std::uint32_t;

inline ZddVar zddVarFor(int position, MoveId move) {
    return static_cast<ZddVar>(position * MAX_MOVE_ID + move);
}

inline std::vector<ZddVar> routeToZddVars(const Route& route) {
    std::vector<ZddVar> vars;
    vars.reserve(route.moves.size());
    for (int i = 0; i < static_cast<int>(route.moves.size()); ++i) {
        vars.push_back(zddVarFor(i, route.moves[i]));
    }
    return vars;
}

struct ZddNode {
    ZddVar var = 0;
    ZddNodeId lo = 0;
    ZddNodeId hi = 0;
};

struct ZddNodeKey {
    ZddVar var = 0;
    ZddNodeId lo = 0;
    ZddNodeId hi = 0;

    bool operator==(const ZddNodeKey& other) const = default;
};

struct ZddNodeKeyHash {
    std::size_t operator()(const ZddNodeKey& key) const {
        std::size_t h = 1469598103934665603ull;
        auto mix = [&](std::size_t v) {
            h ^= v;
            h *= 1099511628211ull;
        };
        mix(key.var);
        mix(key.lo);
        mix(key.hi);
        return h;
    }
};

struct ZddBinaryCacheKey {
    char op = 0;
    ZddNodeId a = 0;
    ZddNodeId b = 0;

    bool operator==(const ZddBinaryCacheKey& other) const = default;
};

struct ZddBinaryCacheKeyHash {
    std::size_t operator()(const ZddBinaryCacheKey& key) const {
        std::size_t h = 1469598103934665603ull;
        auto mix = [&](std::size_t v) {
            h ^= v;
            h *= 1099511628211ull;
        };
        mix(static_cast<unsigned char>(key.op));
        mix(key.a);
        mix(key.b);
        return h;
    }
};

struct ZddStats {
    std::size_t nodeCount = 0;
    std::size_t uniqueNodeCount = 0;
    std::size_t setCount = 0;
};

class ZddManager {
public:
    static constexpr ZddNodeId Zero = 0; // Empty family: contains no sets.
    static constexpr ZddNodeId One = 1;  // Unit family: contains the empty set.

    ZddManager() {
        nodes_.push_back(ZddNode{}); // Zero terminal.
        nodes_.push_back(ZddNode{}); // One terminal.
    }

    ZddNodeId zero() const { return Zero; }
    ZddNodeId one() const { return One; }

    const ZddNode& node(ZddNodeId id) const {
        if (id >= nodes_.size()) {
            throw std::runtime_error("Invalid ZDD node id");
        }
        return nodes_[id];
    }

    std::size_t nodeCount() const {
        return nodes_.size();
    }

    ZddNodeId makeNode(ZddVar var, ZddNodeId lo, ZddNodeId hi) {
        if (hi == Zero) {
            return lo;
        }

        ZddNodeKey key{var, lo, hi};
        auto it = unique_.find(key);
        if (it != unique_.end()) {
            return it->second;
        }

        const ZddNodeId id = static_cast<ZddNodeId>(nodes_.size());
        nodes_.push_back(ZddNode{var, lo, hi});
        unique_.emplace(key, id);
        return id;
    }

    ZddNodeId singletonSet(std::vector<ZddVar> vars) {
        normalizeSet(vars);
        ZddNodeId root = One;
        for (auto it = vars.rbegin(); it != vars.rend(); ++it) {
            root = makeNode(*it, Zero, root);
        }
        return root;
    }

    ZddNodeId insertSet(ZddNodeId root, std::vector<ZddVar> vars) {
        return setUnion(root, singletonSet(std::move(vars)));
    }

    ZddNodeId setUnion(ZddNodeId a, ZddNodeId b) {
        if (a == Zero) return b;
        if (b == Zero) return a;
        if (a == b) return a;
        if (b < a) std::swap(a, b);

        ZddBinaryCacheKey key{'u', a, b};
        auto it = binaryCache_.find(key);
        if (it != binaryCache_.end()) {
            return it->second;
        }

        const ZddVar va = topVar(a);
        const ZddVar vb = topVar(b);
        ZddNodeId out = Zero;
        if (va == vb) {
            out = makeNode(va, setUnion(lo(a), lo(b)), setUnion(hi(a), hi(b)));
        } else if (va < vb) {
            out = makeNode(va, setUnion(lo(a), b), hi(a));
        } else {
            out = makeNode(vb, setUnion(a, lo(b)), hi(b));
        }

        binaryCache_[key] = out;
        return out;
    }

    ZddNodeId setIntersection(ZddNodeId a, ZddNodeId b) {
        if (a == Zero || b == Zero) return Zero;
        if (a == b) return a;
        if (b < a) std::swap(a, b);

        ZddBinaryCacheKey key{'i', a, b};
        auto it = binaryCache_.find(key);
        if (it != binaryCache_.end()) {
            return it->second;
        }

        const ZddVar va = topVar(a);
        const ZddVar vb = topVar(b);
        ZddNodeId out = Zero;
        if (va == vb) {
            out = makeNode(va, setIntersection(lo(a), lo(b)), setIntersection(hi(a), hi(b)));
        } else if (va < vb) {
            out = setIntersection(lo(a), b);
        } else {
            out = setIntersection(a, lo(b));
        }

        binaryCache_[key] = out;
        return out;
    }

    ZddNodeId setDifference(ZddNodeId a, ZddNodeId b) {
        if (a == Zero) return Zero;
        if (b == Zero) return a;
        if (a == b) return Zero;

        ZddBinaryCacheKey key{'d', a, b};
        auto it = binaryCache_.find(key);
        if (it != binaryCache_.end()) {
            return it->second;
        }

        const ZddVar va = topVar(a);
        const ZddVar vb = topVar(b);
        ZddNodeId out = Zero;
        if (va == vb) {
            out = makeNode(va, setDifference(lo(a), lo(b)), setDifference(hi(a), hi(b)));
        } else if (va < vb) {
            out = makeNode(va, setDifference(lo(a), b), hi(a));
        } else {
            out = setDifference(a, lo(b));
        }

        binaryCache_[key] = out;
        return out;
    }

    bool containsSet(ZddNodeId root, std::vector<ZddVar> vars) const {
        normalizeSet(vars);
        std::size_t i = 0;
        ZddNodeId current = root;

        while (current != Zero && current != One) {
            const ZddNode& n = node(current);
            while (i < vars.size() && vars[i] < n.var) {
                return false;
            }

            if (i < vars.size() && vars[i] == n.var) {
                current = n.hi;
                ++i;
            } else {
                current = n.lo;
            }
        }

        return current == One && i == vars.size();
    }

    std::size_t countSets(ZddNodeId root) const {
        std::unordered_map<ZddNodeId, std::size_t> memo;
        return countSetsDfs(root, memo);
    }

    std::vector<std::vector<ZddVar>> enumerateSets(ZddNodeId root, std::size_t limit = std::numeric_limits<std::size_t>::max()) const {
        std::vector<std::vector<ZddVar>> out;
        std::vector<ZddVar> current;
        enumerateSetsDfs(root, current, out, limit);
        return out;
    }

    std::string toDot(ZddNodeId root, const std::unordered_map<ZddVar, std::string>& varLabels = {}) const {
        std::ostringstream os;
        std::unordered_set<ZddNodeId> seen;

        os << "digraph ZDD {\n";
        os << "  rankdir=TB;\n";
        os << "  node [shape=circle, fontname=\"Helvetica\"];\n";
        os << "  z0 [shape=box, label=\"0\", style=filled, fillcolor=\"#f3d7d2\"];\n";
        os << "  z1 [shape=box, label=\"1\", style=filled, fillcolor=\"#d7eadf\"];\n";
        toDotDfs(root, os, seen, varLabels);
        os << "}\n";
        return os.str();
    }

    std::string toSvg(ZddNodeId root, const std::unordered_map<ZddVar, std::string>& varLabels = {}) const {
        std::unordered_set<ZddNodeId> reachableSet;
        collectReachable(root, reachableSet);

        std::vector<ZddNodeId> reachable(reachableSet.begin(), reachableSet.end());
        std::sort(reachable.begin(), reachable.end());

        std::vector<ZddVar> vars;
        for (ZddNodeId id : reachable) {
            if (id != Zero && id != One) {
                vars.push_back(node(id).var);
            }
        }
        std::sort(vars.begin(), vars.end());
        vars.erase(std::unique(vars.begin(), vars.end()), vars.end());

        const int terminalLevel = static_cast<int>(vars.size());
        const int width = std::max(760, 180 + static_cast<int>(reachable.size()) * 36);
        const int rowHeight = 115;
        const int height = 120 + (terminalLevel + 1) * rowHeight;

        std::vector<std::vector<ZddNodeId>> rows(static_cast<std::size_t>(terminalLevel + 1));
        for (ZddNodeId id : reachable) {
            if (id == Zero || id == One) {
                rows[static_cast<std::size_t>(terminalLevel)].push_back(id);
                continue;
            }
            const ZddVar var = node(id).var;
            const auto it = std::find(vars.begin(), vars.end(), var);
            rows[static_cast<std::size_t>(std::distance(vars.begin(), it))].push_back(id);
        }

        std::unordered_map<ZddNodeId, Vec2> pos;
        for (std::size_t row = 0; row < rows.size(); ++row) {
            std::sort(rows[row].begin(), rows[row].end());
            const int count = static_cast<int>(rows[row].size());
            const float gap = count <= 1 ? 0.0f : static_cast<float>(width - 160) / static_cast<float>(count - 1);
            for (int i = 0; i < count; ++i) {
                const float x = count == 1 ? static_cast<float>(width) * 0.5f : 80.0f + gap * static_cast<float>(i);
                const float y = 70.0f + static_cast<float>(row) * static_cast<float>(rowHeight);
                pos[rows[row][static_cast<std::size_t>(i)]] = Vec2{x, y};
            }
        }

        std::ostringstream os;
        os << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width
           << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height << "\">\n";
        os << "<rect width=\"100%\" height=\"100%\" fill=\"#f7f5ef\"/>\n";
        os << "<style>text{font-family:Helvetica,Arial,sans-serif;font-size:13px}.title{font-size:18px;font-weight:700}.node{stroke:#2f302c;stroke-width:1.4}.edge0{stroke:#7b8da5;stroke-width:1.5;stroke-dasharray:6 5;fill:none}.edge1{stroke:#c7372f;stroke-width:1.8;fill:none}.edgeLabel{font-size:11px;fill:#444}</style>\n";
        os << "<defs><marker id=\"arrow1\" markerWidth=\"10\" markerHeight=\"10\" refX=\"8\" refY=\"3\" orient=\"auto\" markerUnits=\"strokeWidth\"><path d=\"M0,0 L0,6 L9,3 z\" fill=\"#c7372f\"/></marker><marker id=\"arrow0\" markerWidth=\"10\" markerHeight=\"10\" refX=\"8\" refY=\"3\" orient=\"auto\" markerUnits=\"strokeWidth\"><path d=\"M0,0 L0,6 L9,3 z\" fill=\"#7b8da5\"/></marker></defs>\n";
        os << "<text class=\"title\" x=\"24\" y=\"32\">ComboLens Route ZDD</text>\n";
        os << "<text x=\"24\" y=\"54\" fill=\"#555\">solid red = include variable, dashed blue = skip variable</text>\n";

        for (ZddNodeId id : reachable) {
            if (id == Zero || id == One) continue;
            const ZddNode& n = node(id);
            drawSvgEdge(os, pos[id], pos[n.lo], "edge0", "0", "arrow0");
            drawSvgEdge(os, pos[id], pos[n.hi], "edge1", "1", "arrow1");
        }

        for (ZddNodeId id : reachable) {
            const Vec2 p = pos[id];
            if (id == Zero || id == One) {
                const char* fill = id == Zero ? "#f3d7d2" : "#d7eadf";
                os << "<rect class=\"node\" x=\"" << p.x - 24 << "\" y=\"" << p.y - 18
                   << "\" width=\"48\" height=\"36\" rx=\"5\" fill=\"" << fill << "\"/>\n";
                os << "<text text-anchor=\"middle\" x=\"" << p.x << "\" y=\"" << p.y + 5 << "\">" << id << "</text>\n";
                continue;
            }

            const ZddNode& n = node(id);
            auto labelIt = varLabels.find(n.var);
            const std::string label = labelIt == varLabels.end() ? std::to_string(n.var) : labelIt->second;
            os << "<circle class=\"node\" cx=\"" << p.x << "\" cy=\"" << p.y << "\" r=\"28\" fill=\"#ffffff\"/>\n";
            os << "<text text-anchor=\"middle\" x=\"" << p.x << "\" y=\"" << p.y - 4 << "\">#" << id << "</text>\n";
            os << "<text text-anchor=\"middle\" x=\"" << p.x << "\" y=\"" << p.y + 12 << "\" font-size=\"11\">" << svgEscape(label) << "</text>\n";
        }

        os << "</svg>\n";
        return os.str();
    }

    ZddStats stats(ZddNodeId root) const {
        std::unordered_set<ZddNodeId> reachable;
        collectReachable(root, reachable);
        return ZddStats{nodes_.size(), reachable.size(), countSets(root)};
    }

private:
    std::vector<ZddNode> nodes_;
    std::unordered_map<ZddNodeKey, ZddNodeId, ZddNodeKeyHash> unique_;
    std::unordered_map<ZddBinaryCacheKey, ZddNodeId, ZddBinaryCacheKeyHash> binaryCache_;

    static void normalizeSet(std::vector<ZddVar>& vars) {
        std::sort(vars.begin(), vars.end());
        vars.erase(std::unique(vars.begin(), vars.end()), vars.end());
    }

    ZddVar topVar(ZddNodeId id) const {
        if (id == Zero || id == One) {
            return std::numeric_limits<ZddVar>::max();
        }
        return node(id).var;
    }

    ZddNodeId lo(ZddNodeId id) const {
        if (id == Zero || id == One) return Zero;
        return node(id).lo;
    }

    ZddNodeId hi(ZddNodeId id) const {
        if (id == Zero || id == One) return Zero;
        return node(id).hi;
    }

    std::size_t countSetsDfs(ZddNodeId id, std::unordered_map<ZddNodeId, std::size_t>& memo) const {
        if (id == Zero) return 0;
        if (id == One) return 1;

        auto it = memo.find(id);
        if (it != memo.end()) {
            return it->second;
        }

        const ZddNode& n = node(id);
        const std::size_t count = countSetsDfs(n.lo, memo) + countSetsDfs(n.hi, memo);
        memo[id] = count;
        return count;
    }

    void enumerateSetsDfs(
        ZddNodeId id,
        std::vector<ZddVar>& current,
        std::vector<std::vector<ZddVar>>& out,
        std::size_t limit
    ) const {
        if (out.size() >= limit || id == Zero) return;
        if (id == One) {
            out.push_back(current);
            return;
        }

        const ZddNode& n = node(id);
        enumerateSetsDfs(n.lo, current, out, limit);
        current.push_back(n.var);
        enumerateSetsDfs(n.hi, current, out, limit);
        current.pop_back();
    }

    void collectReachable(ZddNodeId id, std::unordered_set<ZddNodeId>& out) const {
        if (!out.insert(id).second) return;
        if (id == Zero || id == One) return;
        const ZddNode& n = node(id);
        collectReachable(n.lo, out);
        collectReachable(n.hi, out);
    }

    static std::string svgEscape(const std::string& text) {
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

    static void drawSvgEdge(
        std::ostringstream& os,
        Vec2 from,
        Vec2 to,
        const char* edgeClass,
        const char* label,
        const char* marker
    ) {
        const float midY = (from.y + to.y) * 0.5f;
        os << "<path class=\"" << edgeClass << "\" marker-end=\"url(#" << marker << ")\" d=\"M"
           << from.x << "," << from.y + 28 << " C" << from.x << "," << midY << " "
           << to.x << "," << midY << " " << to.x << "," << to.y - 24 << "\"/>\n";
        os << "<text class=\"edgeLabel\" text-anchor=\"middle\" x=\"" << (from.x + to.x) * 0.5f
           << "\" y=\"" << midY - 4 << "\">" << label << "</text>\n";
    }

    void toDotDfs(
        ZddNodeId id,
        std::ostringstream& os,
        std::unordered_set<ZddNodeId>& seen,
        const std::unordered_map<ZddVar, std::string>& varLabels
    ) const {
        if (id == Zero || id == One || !seen.insert(id).second) return;

        const ZddNode& n = node(id);
        auto labelIt = varLabels.find(n.var);
        const std::string label = labelIt == varLabels.end() ? std::to_string(n.var) : labelIt->second;
        os << "  z" << id << " [label=\"" << label << "\"];\n";
        os << "  z" << id << " -> z" << n.lo << " [label=\"0\", style=dashed, color=\"#7b8da5\"];\n";
        os << "  z" << id << " -> z" << n.hi << " [label=\"1\", color=\"#c7372f\"];\n";
        toDotDfs(n.lo, os, seen, varLabels);
        toDotDfs(n.hi, os, seen, varLabels);
    }
};

struct ZddRouteFamily {
    ZddManager manager;
    ZddNodeId root = ZddManager::Zero;
    std::unordered_map<ZddVar, std::string> varLabels;
};

inline std::string zddVarLabel(int position, MoveId move, const MoveDatabase& db) {
    std::string moveName;
    if (move == MOVE_NONE) {
        moveName = "NONE";
    } else if (move == MOVE_END) {
        moveName = "END";
    } else {
        moveName = db.byId(move).name;
    }
    return std::to_string(position) + ":" + moveName;
}

inline ZddRouteFamily buildRouteZdd(const std::vector<Route>& routes, const MoveDatabase& db) {
    ZddRouteFamily family;
    for (const Route& route : routes) {
        const std::vector<ZddVar> vars = routeToZddVars(route);
        for (int i = 0; i < static_cast<int>(route.moves.size()); ++i) {
            family.varLabels.emplace(vars[static_cast<std::size_t>(i)], zddVarLabel(i, route.moves[static_cast<std::size_t>(i)], db));
        }
        family.root = family.manager.insertSet(family.root, vars);
    }
    return family;
}

inline bool routeInZdd(const ZddRouteFamily& family, const Route& route) {
    return family.manager.containsSet(family.root, routeToZddVars(route));
}

inline std::string routeZddToDot(const ZddRouteFamily& family) {
    return family.manager.toDot(family.root, family.varLabels);
}

inline std::string routeZddToSvg(const ZddRouteFamily& family) {
    return family.manager.toSvg(family.root, family.varLabels);
}

} // namespace combolens
