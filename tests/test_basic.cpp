#include "combolens/example/ExampleMoveSet.hpp"
#include "combolens/abstract/Abstraction.hpp"
#include "combolens/abstract/AbstractTransition.hpp"
#include "combolens/search/RouteSearch.hpp"
#include "combolens/search/StateTransitionGraph.hpp"
#include "combolens/search/ZddEncoding.hpp"

#include <cassert>
#include <iostream>

using namespace combolens;

int main() {
    MoveDatabase db = makeExampleMoveDatabase();
    FrameState initialFrame = makeInitialFrameState();
    SearchState initialAbs = abstractFromFrameState(initialFrame, db);
    SearchNode initial{initialAbs, initialFrame};

    const MoveDef& twoM = db.byId(db.idByName("2M"));
    TransitionResult first = tryApplyMoveHybrid(initial, twoM, db);
    assert(first.valid);
    assert(first.hitVerified);
    assert(first.damage > 0);

    MoveDef tooSlowHeavy = db.byId(db.idByName("5H"));
    tooSlowHeavy.startup = 40;
    tooSlowHeavy.active = 4;
    tooSlowHeavy.hitboxes = {{40, 43, Rect{22, 28, 78, 30}}};
    assert(!tryApplyMoveHybrid(SearchNode{first.next, first.nextRepresentativeFrame}, tooSlowHeavy, db).valid);

    SearchSettings settings;
    settings.maxDepth = MAX_COMBO_LEN;
    settings.forcedStarter = db.idByName("2M");
    settings.routeLimit = 100000;
    SearchResult result = searchRoutes(initial, db, settings);

    assert(result.routeCount > 0);
    assert(!result.bestRoute.moves.empty());
    assert(result.bestRoute.moves.front() == db.idByName("2M"));
    assert(result.bestRoute.moves.back() == MOVE_END);

    auto vars = routeToZddVars(result.bestRoute);
    assert(vars.size() == result.bestRoute.moves.size());

    ZddRouteFamily routeFamily = buildRouteZdd(result.routes, db);
    assert(routeFamily.manager.countSets(routeFamily.root) == result.routes.size());
    assert(routeInZdd(routeFamily, result.bestRoute));
    assert(routeZddToDot(routeFamily).find("digraph ZDD") != std::string::npos);
    assert(routeZddToSvg(routeFamily).find("<svg") != std::string::npos);

    ZddManager zdd;
    const ZddNodeId setA = zdd.singletonSet({1, 3, 5});
    const ZddNodeId setB = zdd.singletonSet({1, 4});
    const ZddNodeId both = zdd.setUnion(setA, setB);
    assert(zdd.countSets(both) == 2);
    assert(zdd.containsSet(both, {1, 3, 5}));
    assert(zdd.containsSet(both, {1, 4}));
    assert(zdd.setIntersection(setA, setB) == zdd.zero());
    assert(zdd.countSets(zdd.setDifference(both, setA)) == 1);

    StateTransitionGraph graph = buildStateTransitionGraph(initial, db, settings);
    assert(!graph.nodes.empty());
    assert(!graph.edges.empty());
    DegenerateLoopReport loops = detectDegenerateLoops(graph);
    assert(!loops.hasDegenerateLoop());
    assert(transitionGraphToDot(graph, db, &loops).find("digraph StateTransitionGraph") != std::string::npos);
    assert(transitionGraphToSvg(graph, db, &loops).find("<svg") != std::string::npos);


    SearchSettings launcherSettings;
    launcherSettings.maxDepth = MAX_COMBO_LEN;
    launcherSettings.forcedStarter = db.idByName("2H");
    launcherSettings.routeLimit = 100000;
    SearchResult launcherResult = searchRoutes(initial, db, launcherSettings);
    const std::vector<MoveId> delayedCancelRoute = {
        db.idByName("2H"),
        db.idByName("jM"),
        db.idByName("jH"),
        db.idByName("236H"),
        db.idByName("Super1"),
        MOVE_END
    };
    bool foundDelayedCancelRoute = false;
    for (const Route& route : launcherResult.routes) {
        if (route.moves == delayedCancelRoute) {
            foundDelayedCancelRoute = true;
            break;
        }
    }
    assert(foundDelayedCancelRoute);
    assert(launcherResult.bestRoute.moves == std::vector<MoveId>({db.idByName("2H"), db.idByName("jM"), db.idByName("236H"), db.idByName("Super1"), MOVE_END}));
    assert(launcherResult.longestRoute.moves == delayedCancelRoute);

        SearchSettings heavyStarterSettings;
    heavyStarterSettings.maxDepth = MAX_COMBO_LEN;
    heavyStarterSettings.forcedStarter = db.idByName("5H");
    heavyStarterSettings.routeLimit = 100000;
    SearchResult heavyStarter = searchRoutes(initial, db, heavyStarterSettings);
    assert(heavyStarter.routeCount > 0);
    assert(heavyStarter.bestRoute.moves.front() == db.idByName("5H"));

    StateTransitionGraph cyclic;
    cyclic.nodes.push_back(TransitionGraphNode{initialAbs, initialFrame, 0});
    cyclic.outgoing.push_back({0});
    cyclic.edges.push_back(TransitionGraphEdge{0, 0, db.idByName("2M"), 1, 1});
    assert(detectDegenerateLoops(cyclic).hasDegenerateLoop());

    std::cout << "basic tests passed\n";
    return 0;
}
