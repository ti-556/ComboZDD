#include "combolens/example/ExampleMoveSet.hpp"
#include "combolens/abstract/Abstraction.hpp"
#include "combolens/search/RouteSearch.hpp"
#include "combolens/search/StateTransitionGraph.hpp"
#include "combolens/search/ZddEncoding.hpp"

#include <cstdlib>
#include <exception>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <optional>
#include <string>

using namespace combolens;

static std::string moveName(const MoveDatabase& db, MoveId id) {
    if (id == MOVE_END) return "END";
    if (id == MOVE_NONE) return "NONE";
    return db.byId(id).name;
}

static void printRoute(const MoveDatabase& db, const Route& r) {
    for (std::size_t i = 0; i < r.moves.size(); ++i) {
        if (i > 0) std::cout << " > ";
        std::cout << moveName(db, r.moves[i]);
    }
}

static const char* oppStateName(SearchOppState s) {
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

static void printUsage(const char* program, const MoveDatabase& db) {
    std::cerr << "Usage: " << program << " [starter-move]\n"
              << "       " << program << " --starter <move-name>\n\n"
              << "Available moves:\n";
    for (const MoveDef& move : db.moves()) {
        std::cerr << "  " << move.name << "\n";
    }
}

static std::optional<MoveId> parseStarterArg(int argc, char** argv, const MoveDatabase& db) {
    if (argc == 1) {
        return db.idByName("2M");
    }

    std::string starterName;
    if (argc == 2) {
        const std::string arg = argv[1];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0], db);
            std::exit(0);
        }
        starterName = arg;
    } else if (argc == 3 && std::string(argv[1]) == "--starter") {
        starterName = argv[2];
    } else {
        printUsage(argv[0], db);
        std::exit(2);
    }

    try {
        return db.idByName(starterName);
    } catch (const std::exception&) {
        std::cerr << "Unknown starter move: " << starterName << "\n\n";
        printUsage(argv[0], db);
        std::exit(2);
    }
}

int main(int argc, char** argv) {
    MoveDatabase db = makeExampleMoveDatabase();
    FrameState initialFrame = makeInitialFrameState();
    SearchState initialAbs = abstractFromFrameState(initialFrame, db);
    SearchNode initial{initialAbs, initialFrame};

    SearchSettings settings;
    settings.maxDepth = MAX_COMBO_LEN;
    settings.forcedStarter = parseStarterArg(argc, argv, db);
    settings.routeLimit = 100000;

    SimSettings simSettings;
    simSettings.gravity = -0.38f;

    SearchResult result = searchRoutes(initial, db, settings, simSettings);
    ZddRouteFamily routeFamily = buildRouteZdd(result.routes, db);
    ZddStats zddStats = routeFamily.manager.stats(routeFamily.root);

    StateTransitionGraph graph = buildStateTransitionGraph(initial, db, settings, simSettings);
    DegenerateLoopReport loopReport = detectDegenerateLoops(graph);

    {
        std::ofstream out("combo_lens_routes.zdd.dot");
        out << routeZddToDot(routeFamily);
    }
    {
        std::ofstream out("combo_lens_routes.zdd.svg");
        out << routeZddToSvg(routeFamily);
    }
    {
        std::ofstream out("combo_lens_state_graph.dot");
        out << transitionGraphToDot(graph, db, &loopReport);
    }
    {
        std::ofstream out("combo_lens_state_graph.svg");
        out << transitionGraphToSvg(graph, db, &loopReport);
    }

    std::cout << "ComboLens v1 demo\n";
    std::cout << "=================\n";
    std::cout << "Moves loaded: " << db.moves().size() << "\n";
    std::cout << "Routes found: " << result.routeCount << "\n";
    std::cout << "Stored routes: " << result.routes.size() << "\n";
    std::cout << "Forced starter: " << moveName(db, *settings.forcedStarter) << "\n";
    std::cout << "Best damage: " << result.bestDamage << "\n";
    std::cout << "ZDD sets: " << zddStats.setCount
              << ", reachable nodes: " << zddStats.uniqueNodeCount
              << ", manager nodes: " << zddStats.nodeCount << "\n";
    std::cout << "State graph: " << graph.nodes.size()
              << " nodes, " << graph.edges.size()
              << " edges, " << loopReport.loops.size()
              << " degenerate loop(s)\n";
    std::cout << "Visualization exports: combo_lens_routes.zdd.dot, combo_lens_routes.zdd.svg, "
              << "combo_lens_state_graph.dot, combo_lens_state_graph.svg\n";

    if (!result.bestRoute.moves.empty()) {
        std::cout << "Best route: ";
        printRoute(db, result.bestRoute);
        std::cout << "\n";

        std::cout << "Final abstract state: opp=" << oppStateName(result.bestRoute.finalState.opp)
                  << ", height=" << static_cast<int>(result.bestRoute.finalState.height)
                  << ", distance=" << static_cast<int>(result.bestRoute.finalState.distance)
                  << ", wallDist=" << static_cast<int>(result.bestRoute.finalState.wallDist)
                  << ", meterBucket=" << static_cast<int>(result.bestRoute.finalState.meter)
                  << ", scalingBucket=" << static_cast<int>(result.bestRoute.finalState.scaling)
                  << ", juggle=" << static_cast<int>(result.bestRoute.finalState.juggle)
                  << "\n";

        std::cout << "ZDD variables: ";
        auto vars = routeToZddVars(result.bestRoute);
        for (ZddVar v : vars) {
            std::cout << v << " ";
        }
        std::cout << "\n";
    }

    std::cout << "\nSample routes:\n";
    const std::size_t count = std::min<std::size_t>(result.routes.size(), 12);
    for (std::size_t i = 0; i < count; ++i) {
        std::cout << std::setw(2) << i + 1 << ". ";
        printRoute(db, result.routes[i]);
        std::cout << " | dmg=" << result.routes[i].stats.totalDamage << "\n";
    }

    return 0;
}
