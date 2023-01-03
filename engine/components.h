#pragma once

#define FOR_LIST_OF_COMPONENTS(X) \
    X(Transform) \
    X(Model) \
    X(FPSControls) \
    X(Observer) \
    X(Player) \
    X(Camera) \
    X(Boid) \
    X(WireframeDebugRenderComp)

#include "components/render.h"
#include "components/controls.h"
#include "components/player.h"
#include "components/boid.h"
