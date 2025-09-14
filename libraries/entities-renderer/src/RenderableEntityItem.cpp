//
//  RenderableEntityItem.cpp
//  interface/src
//
//  Created by Brad Hefta-Gaub on 12/6/13.
//  Copyright 2013 High Fidelity, Inc.
//  Copyright 2024 Overte e.V.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//


#include "RenderableEntityItem.h"

#include <glm/gtx/transform.hpp>
#include <ObjectMotionState.h>
#include <render/TransitionStage.h>

#include "RenderableShapeEntityItem.h"
#include "RenderableModelEntityItem.h"
#include "RenderableTextEntityItem.h"
#include "RenderableImageEntityItem.h"
#include "RenderableWebEntityItem.h"
#include "RenderableParticleEffectEntityItem.h"
#include "RenderableProceduralParticleEffectEntityItem.h"
#include "RenderableLineEntityItem.h"
#include "RenderablePolyLineEntityItem.h"
#include "RenderablePolyVoxEntityItem.h"
#include "RenderableGridEntityItem.h"
#include "RenderableGizmoEntityItem.h"
#include "RenderableLightEntityItem.h"
#include "RenderableZoneEntityItem.h"
#include "RenderableMaterialEntityItem.h"

#include "RenderPipelines.h"

using namespace render;
using namespace render::entities;

void EntityRenderer::initEntityRenderers() {
    REGISTER_ENTITY_TYPE_WITH_FACTORY(Model, RenderableModelEntityItem::factory)
    REGISTER_ENTITY_TYPE_WITH_FACTORY(PolyVox, RenderablePolyVoxEntityItem::factory)
}

const Transform& EntityRenderer::getModelTransform() const {
    return _modelTransform;
}

void EntityRenderer::makeStatusGetters(const EntityItemPointer& entity, Item::Status::Getters& statusGetters) {
    auto nodeList = DependencyManager::get<NodeList>();
    // DANGER: nodeList->getSessionUUID() will return null id when not connected to domain.
    const QUuid& myNodeID = nodeList->getSessionUUID();

    statusGetters.push_back([entity]() -> render::Item::Status::Value {
        uint64_t delta = usecTimestampNow() - entity->getLastEditedFromRemote();
        const float WAIT_THRESHOLD_INV = 1.0f / (0.2f * USECS_PER_SECOND);
        float normalizedDelta = delta * WAIT_THRESHOLD_INV;
        // Status icon will scale from 1.0f down to 0.0f after WAIT_THRESHOLD
        // Color is red if last update is after WAIT_THRESHOLD, green otherwise (120 deg is green)
        return render::Item::Status::Value(1.0f - normalizedDelta, (normalizedDelta > 1.0f ?
            render::Item::Status::Value::GREEN :
            render::Item::Status::Value::RED),
            (unsigned char)render::Item::Status::Icon::PACKET_RECEIVED);
    });

    statusGetters.push_back([entity] () -> render::Item::Status::Value {
        uint64_t delta = usecTimestampNow() - entity->getLastBroadcast();
        const float WAIT_THRESHOLD_INV = 1.0f / (0.4f * USECS_PER_SECOND);
        float normalizedDelta = delta * WAIT_THRESHOLD_INV;
        // Status icon will scale from 1.0f down to 0.0f after WAIT_THRESHOLD
        // Color is Magenta if last update is after WAIT_THRESHOLD, cyan otherwise (180 deg is green)
        return render::Item::Status::Value(1.0f - normalizedDelta, (normalizedDelta > 1.0f ?
            render::Item::Status::Value::MAGENTA :
            render::Item::Status::Value::CYAN),
            (unsigned char)render::Item::Status::Icon::PACKET_SENT);
    });

    statusGetters.push_back([entity] () -> render::Item::Status::Value {
        ObjectMotionState* motionState = static_cast<ObjectMotionState*>(entity->getPhysicsInfo());
        if (motionState && motionState->isActive()) {
            return render::Item::Status::Value(1.0f, render::Item::Status::Value::BLUE,
                (unsigned char)render::Item::Status::Icon::ACTIVE_IN_BULLET);
        }
        return render::Item::Status::Value(0.0f, render::Item::Status::Value::BLUE,
            (unsigned char)render::Item::Status::Icon::ACTIVE_IN_BULLET);
    });

    statusGetters.push_back([entity, myNodeID] () -> render::Item::Status::Value {
        bool weOwnSimulation = entity->getSimulationOwner().matchesValidID(myNodeID);
        bool otherOwnSimulation = !weOwnSimulation && !entity->getSimulationOwner().isNull();

        if (weOwnSimulation) {
            return render::Item::Status::Value(1.0f, render::Item::Status::Value::BLUE,
                (unsigned char)render::Item::Status::Icon::SIMULATION_OWNER);
        } else if (otherOwnSimulation) {
            return render::Item::Status::Value(1.0f, render::Item::Status::Value::RED,
                (unsigned char)render::Item::Status::Icon::OTHER_SIMULATION_OWNER);
        }
        return render::Item::Status::Value(0.0f, render::Item::Status::Value::BLUE,
            (unsigned char)render::Item::Status::Icon::SIMULATION_OWNER);
    });

    statusGetters.push_back([entity] () -> render::Item::Status::Value {
        if (entity->hasActions()) {
            return render::Item::Status::Value(1.0f, render::Item::Status::Value::GREEN,
                (unsigned char)render::Item::Status::Icon::HAS_ACTIONS);
        }
        return render::Item::Status::Value(0.0f, render::Item::Status::Value::GREEN,
            (unsigned char)render::Item::Status::Icon::HAS_ACTIONS);
    });

    statusGetters.push_back([entity] () -> render::Item::Status::Value {
        if (entity->isAvatarEntity()) {
            if (entity->isMyAvatarEntity()) {
                return render::Item::Status::Value(1.0f, render::Item::Status::Value::GREEN,
                    (unsigned char)render::Item::Status::Icon::ENTITY_HOST_TYPE);
            } else {
                return render::Item::Status::Value(1.0f, render::Item::Status::Value::RED,
                    (unsigned char)render::Item::Status::Icon::ENTITY_HOST_TYPE);
            }
        } else if (entity->isLocalEntity()) {
            return render::Item::Status::Value(1.0f, render::Item::Status::Value::BLUE,
                (unsigned char)render::Item::Status::Icon::ENTITY_HOST_TYPE);
        }
        return render::Item::Status::Value(0.0f, render::Item::Status::Value::GREEN,
            (unsigned char)render::Item::Status::Icon::ENTITY_HOST_TYPE);
    });
}


template <typename T> 
std::shared_ptr<T> make_renderer(const EntityItemPointer& entity) {
    // We want to use deleteLater so that renderer destruction gets pushed to the main thread
    return std::shared_ptr<T>(new T(entity), [](T* ptr) { ptr->deleteLater(); });
}

EntityRenderer::EntityRenderer(const EntityItemPointer& entity) :
    _fadeInProperties(entity->getFadeInProperties()),
    _fadeInMode((ComponentMode)entity->getFadeInMode()),
    _created(entity->getCreated()),
    _entity(entity),
    _entityID(entity->getID()) {
}

EntityRenderer::~EntityRenderer() {}

//
// Smart payload proxy members, implementing the payload interface
//

Item::Bound EntityRenderer::getBound(RenderArgs* args) {
    auto bound = _bound;
    if (_billboardMode != BillboardMode::NONE) {
        glm::vec3 dimensions = bound.getScale();
        float max = glm::max(dimensions.x, glm::max(dimensions.y, dimensions.z));
        const float SQRT_2 = 1.41421356237f;
        bound.setScaleStayCentered(glm::vec3(SQRT_2 * max));
    }
    return bound;
}

ShapeKey EntityRenderer::getShapeKey() {
    ShapeKey::Builder builder = ShapeKey::Builder().withOwnPipeline();
    if (_primitiveMode == PrimitiveMode::LINES) {
        builder.withWireframe();
    }
    return builder.build();
}

render::hifi::Tag EntityRenderer::getTagMask() const {
    render::hifi::Tag mask = render::hifi::TAG_NONE;
    mask = (render::hifi::Tag)(mask | (!_cauterized * render::hifi::TAG_MAIN_VIEW));
    mask = (render::hifi::Tag)(mask | (_isVisibleInSecondaryCamera * render::hifi::TAG_SECONDARY_VIEW));
    return mask;
}

render::hifi::Layer EntityRenderer::getHifiRenderLayer() const {
    switch (_renderLayer) {
        case RenderLayer::WORLD:
            return render::hifi::LAYER_3D;
        case RenderLayer::FRONT:
            return render::hifi::LAYER_3D_FRONT;
        case RenderLayer::HUD:
            return render::hifi::LAYER_3D_HUD;
        default:
            return render::hifi::LAYER_3D;
    }
}

ItemKey EntityRenderer::getKey() {
    ItemKey::Builder builder =
        ItemKey::Builder().withTypeShape().withTypeMeta().withTagBits(getTagMask()).withLayer(getHifiRenderLayer());

    if (isTransparent()) {
        builder.withTransparent();
    } else if (_canCastShadow) {
        builder.withShadowCaster();
    }

    if (_cullWithParent) {
        builder.withSubMetaCulled();
    }

    if (_mirrorMode == MirrorMode::MIRROR || (_mirrorMode == MirrorMode::PORTAL && !_portalExitID.isNull())) {
        builder.withMirror();
    }

    if (!_visible) {
        builder.withInvisible();
    }

    updateItemKeyBuilderFromMaterials(builder);

    return builder;
}

uint32_t EntityRenderer::metaFetchMetaSubItems(ItemIDs& subItems) const {
    if (Item::isValidID(_renderItemID)) {
        subItems.emplace_back(_renderItemID);
        return 1;
    }
    return 0;
}

bool EntityRenderer::passesZoneOcclusionTest(const std::unordered_set<QUuid>& containingZones) const {
    if (!_renderWithZones.isEmpty()) {
        if (!containingZones.empty()) {
            for (auto renderWithZone : _renderWithZones) {
                if (containingZones.find(renderWithZone) != containingZones.end()) {
                    return true;
                }
            }
        }
        return false;
    }
    return true;
}

ItemID EntityRenderer::computeMirrorView(ViewFrustum& viewFrustum) const {
    glm::vec3 inPropertiesPosition;
    glm::quat inPropertiesRotation;
    MirrorMode mirrorMode;
    QUuid portalExitID;
    withReadLock([&]{
        inPropertiesPosition = _entity->getWorldPosition();
        inPropertiesRotation = _entity->getWorldOrientation();
        mirrorMode = _mirrorMode;
        portalExitID = _portalExitID;
    });
    return computeMirrorViewOperator(viewFrustum, inPropertiesPosition, inPropertiesRotation, mirrorMode, portalExitID);
}

ItemID EntityRenderer::computeMirrorViewOperator(ViewFrustum& viewFrustum, const glm::vec3& inPropertiesPosition, const glm::quat& inPropertiesRotation,
                                                 MirrorMode mirrorMode, const QUuid& portalExitID) {
    glm::mat4 inToWorld = glm::translate(inPropertiesPosition) * glm::mat4_cast(inPropertiesRotation);
    glm::mat4 worldToIn = glm::inverse(inToWorld);

    glm::vec3 outPropertiesPosition = inPropertiesPosition;
    glm::quat outPropertiesRotation = inPropertiesRotation;
    glm::mat4 outToWorld = inToWorld;
    bool foundPortalExit = false;
    if (mirrorMode == MirrorMode::PORTAL && !portalExitID.isNull()) {
        auto renderer = DependencyManager::get<EntityTreeRenderer>();
        if (renderer) {
            if (auto renderable = renderer->renderableForEntityId(portalExitID)) {
                renderable->withReadLock([&] {
                    outPropertiesPosition = renderable->_entity->getWorldPosition();
                    outPropertiesRotation = renderable->_entity->getWorldOrientation();
                });

                outToWorld = glm::translate(outPropertiesPosition) * glm::mat4_cast(outPropertiesRotation);
                foundPortalExit = true;
            }
        }
    }

    // get mirror camera position by reflecting main camera position's z coordinate in mirror space
    glm::vec3 cameraPositionWorld = viewFrustum.getPosition();
    glm::vec3 cameraPositionIn = vec3(worldToIn * vec4(cameraPositionWorld, 1.0f));
    glm::vec3 mirrorCameraPositionIn = vec3(cameraPositionIn.x, cameraPositionIn.y, -cameraPositionIn.z);
    if (foundPortalExit) {
        // portals also flip over x
        mirrorCameraPositionIn.x *= -1.0f;
    }
    glm::vec3 mirrorCameraPositionWorld = vec3(outToWorld * vec4(mirrorCameraPositionIn, 1.0f));

    // get mirror camera rotation by reflecting main camera rotation in mirror space
    // TODO: we are assuming here that UP is world y-axis
    glm::quat mainCameraRotationWorld = viewFrustum.getOrientation();
    glm::quat mainCameraRotationMirror = worldToIn * glm::mat4_cast(mainCameraRotationWorld);
    glm::quat mirrorCameraRotationMirror = glm::quat(mainCameraRotationMirror.w, -mainCameraRotationMirror.x, -mainCameraRotationMirror.y, mainCameraRotationMirror.z) *
        glm::angleAxis((float)M_PI, glm::vec3(0, 1, 0));
    if (foundPortalExit) {
        // portals also flip over x
        mirrorCameraRotationMirror = glm::quat(mirrorCameraRotationMirror.w, mirrorCameraRotationMirror.x, -mirrorCameraRotationMirror.y, -mirrorCameraRotationMirror.z);
    }
    glm::quat mirrorCameraRotationWorld = outToWorld * glm::mat4_cast(mirrorCameraRotationMirror);

    viewFrustum.setPosition(mirrorCameraPositionWorld);
    viewFrustum.setOrientation(mirrorCameraRotationWorld);

    // modify the near clip plane to be the XY plane of the mirror
    // from: https://terathon.com/lengyel/Lengyel-Oblique.pdf
    glm::mat4 view = viewFrustum.getView();
    glm::mat4 projection = viewFrustum.getProjection();

    //Find the camera-space 4D reflection plane vector
    glm::vec3 cameraSpacePosition = glm::inverse(view) * glm::vec4(outPropertiesPosition, 1.0f);
    glm::vec3 cameraSpaceNormal = glm::transpose(view) * (outPropertiesRotation * glm::vec4(0, 0, -1, 0));
    glm::vec4 clipPlane = glm::vec4(cameraSpaceNormal, -glm::dot(cameraSpaceNormal, cameraSpacePosition));
    // Make sure we pick the direction facing away from us
    if (clipPlane.w > 0.0f) {
        clipPlane *= -1.0f;
    }

    // Calculate the clip-space corner point opposite the clipping plane
    // as (sign(clipPlane.x), sign(clipPlane.y), 1, 1) and
    // transform it into camera space by multiplying it
    // by the inverse of the projection matrix
    glm::vec4 q;
    q.x = (glm::sign(clipPlane.x) + projection[0][2]) / projection[0][0];
    q.y = (glm::sign(clipPlane.y) + projection[1][2]) / projection[1][1];
    q.z = -1.0f;
    q.w = (1.0f + projection[2][2]) / projection[2][3];

    // Calculate the scaled plane vector
    glm::vec4 c = (2.0f / glm::dot(clipPlane, q)) * clipPlane;

    // Replace the third row of the projection matrix
    projection[0][2] = c.x;
    projection[1][2] = c.y;
    projection[2][2] = c.z + 1.0f;
    projection[3][2] = c.w;

    viewFrustum.setProjection(projection, true);

    return foundPortalExit ? DependencyManager::get<EntityTreeRenderer>()->renderableIdForEntityId(portalExitID) : Item::INVALID_ITEM_ID;
}

HighlightStyle EntityRenderer::getOutlineStyle(const ViewFrustum& viewFrustum, const size_t height) const {
    std::lock_guard<std::mutex> lock(_materialsLock);
    auto materials = _materials.find("0");
    if (materials != _materials.end()) {
        glm::vec3 position;
        withReadLock([&] {
            position = _renderTransform.getTranslation();
        });
        return HighlightStyle::calculateOutlineStyle(materials->second.getOutlineWidthMode(), materials->second.getOutlineWidth(),
                                                     materials->second.getOutline(), position, viewFrustum, height);
    }
    return HighlightStyle();
}

FadeProperties EntityRenderer::getFadeProperties(const TransitionType type) const {
    FadeProperties fadeProperties;
    switch (type) {
        case TransitionType::ELEMENT_ENTER_DOMAIN: {
            if (_fadeInMode == ComponentMode::COMPONENT_MODE_ENABLED) {
                fadeProperties = {
                    _fadeInProperties.getDuration(),
                    _fadeInProperties.getTiming(),
                    _fadeInProperties.getNoiseSpeed(),
                    1.0f / _fadeInProperties.getNoiseSize(),
                    _fadeInProperties.getNoiseLevel(),
                    1.0f / _fadeInProperties.getBaseSize(),
                    _fadeInProperties.getBaseLevel(),
                    vec4(vec3(_fadeInProperties.getEdgeInnerColor()) / 255.0f, _fadeInProperties.getEdgeInnerAlpha()),
                    vec4(vec3(_fadeInProperties.getEdgeOuterColor()) / 255.0f, _fadeInProperties.getEdgeOuterAlpha()),
                    _fadeInProperties.getEdgeWidth(),
                    _fadeInProperties.getInverted()
                };
            } else if (auto renderer = DependencyManager::get<EntityTreeRenderer>()) {
                fadeProperties = renderer->getLayeredZoneFadeProperties(type);
            }
            break;
        }
        case TransitionType::ELEMENT_LEAVE_DOMAIN: {
            if (_fadeOutMode == ComponentMode::COMPONENT_MODE_ENABLED) {
                fadeProperties = {
                    _fadeOutProperties.getDuration(),
                    _fadeOutProperties.getTiming(),
                    _fadeOutProperties.getNoiseSpeed(),
                    1.0f / _fadeOutProperties.getNoiseSize(),
                    _fadeOutProperties.getNoiseLevel(),
                    1.0f / _fadeOutProperties.getBaseSize(),
                    _fadeOutProperties.getBaseLevel(),
                    vec4(vec3(_fadeOutProperties.getEdgeInnerColor()) / 255.0f, _fadeOutProperties.getEdgeInnerAlpha()),
                    vec4(vec3(_fadeOutProperties.getEdgeOuterColor()) / 255.0f, _fadeOutProperties.getEdgeOuterAlpha()),
                    _fadeOutProperties.getEdgeWidth(),
                    _fadeOutProperties.getInverted()
                };
            } else if (auto renderer = DependencyManager::get<EntityTreeRenderer>()) {
                fadeProperties = renderer->getLayeredZoneFadeProperties(type);
            }
            break;
        }
        default:
            break;
    }
    return fadeProperties;
}

void EntityRenderer::render(RenderArgs* args) {
    if (!isValidRenderItem()) {
        return;
    }

    if (_visible && (!_cauterized || args->_renderMode != RenderArgs::RenderMode::DEFAULT_RENDER_MODE || args->_mirrorDepth > 0)) {
        doRender(args);
    }
}

//
// Methods called by the EntityTreeRenderer
//

EntityRenderer::Pointer EntityRenderer::addToScene(EntityTreeRenderer& renderer, const EntityItemPointer& entity, const ScenePointer& scene, Transaction& transaction) {
    EntityRenderer::Pointer result;
    if (!entity) {
        return result;
    }

    using Type = EntityTypes::EntityType_t;
    auto type = entity->getType();
    switch (type) {

        case Type::Shape:
        case Type::Box:
        case Type::Sphere:
            result = make_renderer<ShapeEntityRenderer>(entity);
            break;

        case Type::Model:
            result = make_renderer<ModelEntityRenderer>(entity);
            break;

        case Type::Text:
            result = make_renderer<TextEntityRenderer>(entity);
            break;

        case Type::Image:
            result = make_renderer<ImageEntityRenderer>(entity);
            break;

        case Type::Web:
            if (!nsightActive()) {
                result = make_renderer<WebEntityRenderer>(entity);
            }
            break;

        case Type::ParticleEffect:
            result = make_renderer<ParticleEffectEntityRenderer>(entity);
            break;

        case Type::ProceduralParticleEffect:
            result = make_renderer<ProceduralParticleEffectEntityRenderer>(entity);
            break;

        case Type::Line:
            result = make_renderer<LineEntityRenderer>(entity);
            break;

        case Type::PolyLine:
            result = make_renderer<PolyLineEntityRenderer>(entity);
            break;

        case Type::PolyVox:
            result = make_renderer<PolyVoxEntityRenderer>(entity);
            break;

        case Type::Grid:
            result = make_renderer<GridEntityRenderer>(entity);
            break;

        case Type::Gizmo:
            result = make_renderer<GizmoEntityRenderer>(entity);
            break;

        case Type::Light:
            result = make_renderer<LightEntityRenderer>(entity);
            break;

        case Type::Zone:
            result = make_renderer<ZoneEntityRenderer>(entity);
            break;

        case Type::Material:
            result = make_renderer<MaterialEntityRenderer>(entity);
            break;

        default:
            break;
    }

    if (result) {
        result->addToScene(scene, transaction);
    }

    return result;
}

bool EntityRenderer::addToScene(const ScenePointer& scene, Transaction& transaction) {
    _renderItemID = scene->allocateID();
    // Complicated series of trusses
    auto renderPayload = std::make_shared<PayloadProxyInterface::ProxyPayload>(shared_from_this());
    Item::Status::Getters statusGetters;
    makeStatusGetters(_entity, statusGetters);
    renderPayload->addStatusGetters(statusGetters);
    transaction.resetItem(_renderItemID, renderPayload);
    auto renderer = DependencyManager::get<EntityTreeRenderer>();
    if (renderer) {
        if (_fadeInMode == ComponentMode::COMPONENT_MODE_ENABLED ||
            (_fadeInMode == ComponentMode::COMPONENT_MODE_INHERIT && renderer->layeredZonesHaveFade(TransitionType::ELEMENT_ENTER_DOMAIN))) {
            fade(transaction, TransitionType::ELEMENT_ENTER_DOMAIN);
        }
    }
    onAddToScene(_entity);
    updateInScene(scene, transaction);
    return true;
}

void EntityRenderer::removeFromScene(const ScenePointer& scene, Transaction& transaction) {
    onRemoveFromScene(_entity);
    transaction.removeItem(_renderItemID);
    Item::clearID(_renderItemID);
}

void EntityRenderer::updateInScene(const ScenePointer& scene, Transaction& transaction) {
    DETAILED_PROFILE_RANGE(simulation_physics, __FUNCTION__);
    if (!isValidRenderItem()) {
        return;
    }
    _updateTime = usecTimestampNow();

    doRenderUpdateSynchronous(scene, transaction, _entity);
    transaction.updateItem<PayloadProxyInterface>(_renderItemID, [this](PayloadProxyInterface& self) {
        if (!isValidRenderItem()) {
            return;
        }
        // Happens on the render thread.  Classes should use
        doRenderUpdateAsynchronous(_entity);
    });
}

void EntityRenderer::fade(render::Transaction& transaction, TransitionType type) {
    transaction.resetTransitionOnItem(_renderItemID, type);
}

//
// Internal methods
//

// Returns true if the item needs to have updateInscene called because of internal rendering 
// changes (animation, fading, etc)
bool EntityRenderer::needsRenderUpdate() const {
    if (isFading()) {
        return true;
    }

    if (_prevIsTransparent != isTransparent()) {
        return true;
    }

    return needsRenderUpdateFromEntity(_entity);
}

Transform EntityRenderer::getTransformToCenterWithMaybeOnlyLocalRotation(const EntityItemPointer& entity, bool& success) const {
    return entity->getBillboardMode() == BillboardMode::NONE ? entity->getTransformToCenter(success) :
        entity->getTransformToCenterWithOnlyLocalRotation(success);
}

// Returns true if the item in question needs to have updateInScene called because of changes in the entity
bool EntityRenderer::needsRenderUpdateFromEntity(const EntityItemPointer& entity) const {
    if (entity->needsRenderUpdate()) {
        return true;
    }

    if (!entity->isVisuallyReady()) {
        return true;
    }

    bool success = false;
    auto bound = entity->getAABox(success);
    if (success && _bound != bound) {
        return true;
    }

    auto newModelTransform = getTransformToCenterWithMaybeOnlyLocalRotation(entity, success);
    // FIXME can we use a stale model transform here?
    if (success && newModelTransform != _modelTransform) {
        return true;
    }

    if (_moving != entity->isMovingRelativeToParent()) {
        return true;
    }

    if (entity->needsZoneOcclusionUpdate()) {
        return true;
    }

    return false;
}

void EntityRenderer::updateModelTransformAndBound(const EntityItemPointer& entity) {
    bool success = false;
    auto newModelTransform = getTransformToCenterWithMaybeOnlyLocalRotation(entity, success);
    if (success) {
        _modelTransform = newModelTransform;
    }

    success = false;
    auto bound = entity->getAABox(success);
    if (success) {
        _bound = bound;
    }
}

void EntityRenderer::doRenderUpdateSynchronous(const ScenePointer& scene, Transaction& transaction, const EntityItemPointer& entity) {
    DETAILED_PROFILE_RANGE(simulation_physics, __FUNCTION__);
    withWriteLock([&] {
        auto transparent = isTransparent();
        auto fading = isFading();
        if (fading || _prevIsTransparent != transparent || !entity->isVisuallyReady()) {
            emit requestRenderUpdate();
        }
        if (fading) {
            _isFading = Interpolate::calculateFadeRatio(_fadeStartTime) < 1.0f;
        }

        _prevIsTransparent = transparent;

        updateModelTransformAndBound(entity);

        _moving = entity->isMovingRelativeToParent();
        _visible = entity->getVisible();
        entity->setNeedsRenderUpdate(false);
    });
}

void EntityRenderer::doRenderUpdateAsynchronous(const EntityItemPointer& entity) {
    setIsVisibleInSecondaryCamera(entity->getIsVisibleInSecondaryCamera());
    setRenderLayer(entity->getRenderLayer());
    _billboardMode = entity->getBillboardMode();
    _primitiveMode = entity->getPrimitiveMode();
    _canCastShadow = entity->getCanCastShadow();
    setCullWithParent(entity->getCullWithParent());
    _cauterized = entity->getCauterized();
    setMirrorMode(entity->getMirrorMode());
    setPortalExitID(entity->getPortalExitID());
    if (entity->needsZoneOcclusionUpdate()) {
        entity->resetNeedsZoneOcclusionUpdate();
        _renderWithZones = entity->getRenderWithZones();
    }
    _fadeOutProperties = entity->getFadeOutProperties();
    _fadeOutMode = (ComponentMode)entity->getFadeOutMode();
}

void EntityRenderer::onAddToScene(const EntityItemPointer& entity) {
    QObject::connect(this, &EntityRenderer::requestRenderUpdate, this, [this] {
        auto renderer = DependencyManager::get<EntityTreeRenderer>();
        if (renderer) {
            renderer->onEntityChanged(_entity->getID());
        }
    }, Qt::QueuedConnection);
    _changeHandlerId = entity->registerChangeHandler([](const EntityItemID& changedEntity) {
        auto renderer = DependencyManager::get<EntityTreeRenderer>();
        if (renderer) {
            auto renderable = renderer->renderableForEntityId(changedEntity);
            if (renderable && renderable->needsRenderUpdate()) {
                renderer->onEntityChanged(changedEntity);
            }
        }
    });
}

void EntityRenderer::onRemoveFromScene(const EntityItemPointer& entity) { 
    entity->deregisterChangeHandler(_changeHandlerId);
    QObject::disconnect(this, &EntityRenderer::requestRenderUpdate, this, nullptr);
}

void EntityRenderer::addMaterial(graphics::MaterialLayer material, const std::string& parentMaterialName) {
    std::lock_guard<std::mutex> lock(_materialsLock);
    _materials[parentMaterialName].push(material);
    emit requestRenderUpdate();
}

void EntityRenderer::removeMaterial(graphics::MaterialPointer material, const std::string& parentMaterialName) {
    std::lock_guard<std::mutex> lock(_materialsLock);
    _materials[parentMaterialName].remove(material);
    emit requestRenderUpdate();
}

graphics::MaterialPointer EntityRenderer::getTopMaterial() {
    std::lock_guard<std::mutex> lock(_materialsLock);
    auto materials = _materials.find("0");
    if (materials != _materials.end() && materials->second.size() > 0) {
        return materials->second.top().material;
    }
    return nullptr;
}

EntityRenderer::Pipeline EntityRenderer::getPipelineType(const graphics::MultiMaterial& materials) {
    if (_mirrorMode == MirrorMode::MIRROR || (_mirrorMode == MirrorMode::PORTAL && !_portalExitID.isNull())) {
        return Pipeline::MIRROR;
    }

    if (materials.size() > 0 && materials.top().material && materials.top().material->isProcedural() && materials.top().material->isReady()) {
        return Pipeline::PROCEDURAL;
    }

    graphics::MaterialKey drawMaterialKey = materials.getMaterialKey();
    if (materials.isMToon() || drawMaterialKey.isEmissive() || drawMaterialKey.isMetallic() || drawMaterialKey.isScattering()) {
        return Pipeline::MATERIAL;
    }

    // If the material is using any map, we need to use a material ShapeKey
    for (int i = 0; i < graphics::Material::MapChannel::NUM_MAP_CHANNELS; i++) {
        if (drawMaterialKey.isMapChannel(graphics::Material::MapChannel(i))) {
            return Pipeline::MATERIAL;
        }
    }
    return Pipeline::SIMPLE;
}

bool EntityRenderer::needsRenderUpdateFromMaterials() const {
    MaterialMap::const_iterator materials;
    {
        std::lock_guard<std::mutex> lock(_materialsLock);
        materials = _materials.find("0");

        if (materials == _materials.cend()) {
            return false;
        }
    }

    if (materials->second.shouldUpdate()) {
        return true;
    }

    if (materials->second.size() > 0 && materials->second.top().material && materials->second.top().material->isProcedural() && materials->second.top().material->isReady()) {
        auto procedural = std::static_pointer_cast<graphics::ProceduralMaterial>(materials->second.top().material);
        if (procedural->isFading()) {
            return true;
        }
    }

    return false;
}

void EntityRenderer::updateMaterials(bool baseMaterialChanged) {
    MaterialMap::iterator materials;
    {
        std::lock_guard<std::mutex> lock(_materialsLock);
        materials = _materials.find("0");

        if (materials == _materials.end()) {
            return;
        }
    }

    if (baseMaterialChanged) {
        materials->second.setNeedsUpdate(true);
    }

    bool requestUpdate = false;
    if (materials->second.size() > 0 && materials->second.top().material && materials->second.top().material->isProcedural() && materials->second.top().material->isReady()) {
        auto procedural = std::static_pointer_cast<graphics::ProceduralMaterial>(materials->second.top().material);
        if (procedural->isFading()) {
            procedural->setIsFading(Interpolate::calculateFadeRatio(procedural->getFadeStartTime()) < 1.0f);
            requestUpdate = true;
        }
    }

    if (materials->second.shouldUpdate()) {
        RenderPipelines::updateMultiMaterial(materials->second);
        requestUpdate = true;
    }

    if (requestUpdate) {
        emit requestRenderUpdate();
    }
}

bool EntityRenderer::materialsTransparent() const {
    MaterialMap::const_iterator materials;
    {
        std::lock_guard<std::mutex> lock(_materialsLock);
        materials = _materials.find("0");

        if (materials == _materials.cend()) {
            return false;
        }
    }

    if (materials->second.size() > 0 && materials->second.top().material) {
        if (materials->second.top().material->isProcedural() && materials->second.top().material->isReady()) {
            auto procedural = std::static_pointer_cast<graphics::ProceduralMaterial>(materials->second.top().material);
            if (procedural->isFading()) {
                return true;
            }
        }

        if (materials->second.getMaterialKey().isTranslucent()) {
            return true;
        }
    }

    return false;
}

Item::Bound EntityRenderer::getMaterialBound(RenderArgs* args) {
    MaterialMap::iterator materials;
    {
        std::lock_guard<std::mutex> lock(_materialsLock);
        materials = _materials.find("0");

        if (materials == _materials.end()) {
            return EntityRenderer::getBound(args);
        }
    }

    if (materials->second.size() > 0 && materials->second.top().material && materials->second.top().material->isProcedural() && materials->second.top().material->isReady()) {
        auto procedural = std::static_pointer_cast<graphics::ProceduralMaterial>(materials->second.top().material);
        if (procedural->hasVertexShader() && procedural->hasBoundOperator()) {
            return procedural->getBound(args);
        }
    }

    return EntityRenderer::getBound(args);
}

void EntityRenderer::updateItemKeyBuilderFromMaterials(ItemKey::Builder& builder) {
    MaterialMap::iterator materials;
    {
        std::lock_guard<std::mutex> lock(_materialsLock);
        materials = _materials.find("0");

        if (materials != _materials.end()) {
            if (materials->second.shouldUpdate()) {
                RenderPipelines::updateMultiMaterial(materials->second);
            }
        } else {
            return;
        }
    }

    if (materials->second.hasOutline()) {
        builder.withOutline();
    }
}

void EntityRenderer::updateShapeKeyBuilderFromMaterials(ShapeKey::Builder& builder) {
    MaterialMap::iterator materials;
    {
        std::lock_guard<std::mutex> lock(_materialsLock);
        materials = _materials.find("0");

        if (materials != _materials.end()) {
            if (materials->second.shouldUpdate()) {
                RenderPipelines::updateMultiMaterial(materials->second);
            }
        } else {
            return;
        }
    }

    if (isTransparent()) {
        builder.withTranslucent();
    }

    if (_primitiveMode == PrimitiveMode::LINES) {
        builder.withWireframe();
    }

    builder.withCullFaceMode(materials->second.getCullFaceMode());

    graphics::MaterialKey drawMaterialKey = materials->second.getMaterialKey();
    // TODO: support lit/unlit mtoon
    if (!materials->second.isMToon() && drawMaterialKey.isUnlit()) {
        builder.withUnlit();
    }

    auto pipelineType = getPipelineType(materials->second);
    if (pipelineType == Pipeline::MATERIAL) {
        builder.withMaterial();

        builder.withLayers(materials->second.getLayers());

        if (drawMaterialKey.isNormalMap()) {
            builder.withTangents();
        }
        if (!materials->second.isMToon()) {
            if (drawMaterialKey.isLightMap()) {
                builder.withLightMap();
            }
        } else {
            builder.withMToon();
        }

        if (materials->second.size() > 0 && materials->second.top().material &&
            (MaterialMappingMode)materials->second.top().material->getMaterialParams().x == MaterialMappingMode::TRIPLANAR) {
            builder.withTriplanar();
        } else if (materials->second.isSplatMap()) {
            builder.withSplatMap();
        }
    } else if (pipelineType == Pipeline::PROCEDURAL) {
        builder.withOwnPipeline();
    }
}

glm::vec4 EntityRenderer::calculatePulseColor(const glm::vec4& color, const PulsePropertyGroup& pulseProperties, quint64 start) {
    if (pulseProperties.getPeriod() == 0.0f || (pulseProperties.getColorMode() == PulseMode::NONE && pulseProperties.getAlphaMode() == PulseMode::NONE)) {
        return color;
    }

    float t = ((float)(usecTimestampNow() - start)) / ((float)USECS_PER_SECOND);
    float pulse = 0.5f * (cosf(t * (2.0f * (float)M_PI) / pulseProperties.getPeriod()) + 1.0f) * (pulseProperties.getMax() - pulseProperties.getMin()) + pulseProperties.getMin();
    float outPulse = (1.0f - pulse);

    glm::vec4 result = color;
    if (pulseProperties.getColorMode() == PulseMode::IN_PHASE) {
        result.r *= pulse;
        result.g *= pulse;
        result.b *= pulse;
    } else if (pulseProperties.getColorMode() == PulseMode::OUT_PHASE) {
        result.r *= outPulse;
        result.g *= outPulse;
        result.b *= outPulse;
    }

    if (pulseProperties.getAlphaMode() == PulseMode::IN_PHASE) {
        result.a *= pulse;
    } else if (pulseProperties.getAlphaMode() == PulseMode::OUT_PHASE) {
        result.a *= outPulse;
    }

    return result;
}

glm::vec3 EntityRenderer::calculatePulseColor(const glm::vec3& color, const PulsePropertyGroup& pulseProperties, quint64 start) {
    if (pulseProperties.getPeriod() == 0.0f || (pulseProperties.getColorMode() == PulseMode::NONE && pulseProperties.getAlphaMode() == PulseMode::NONE)) {
        return color;
    }

    float t = ((float)(usecTimestampNow() - start)) / ((float)USECS_PER_SECOND);
    float pulse = 0.5f * (cosf(t * (2.0f * (float)M_PI) / pulseProperties.getPeriod()) + 1.0f) * (pulseProperties.getMax() - pulseProperties.getMin()) + pulseProperties.getMin();
    float outPulse = (1.0f - pulse);

    glm::vec3 result = color;
    if (pulseProperties.getColorMode() == PulseMode::IN_PHASE) {
        result.r *= pulse;
        result.g *= pulse;
        result.b *= pulse;
    } else if (pulseProperties.getColorMode() == PulseMode::OUT_PHASE) {
        result.r *= outPulse;
        result.g *= outPulse;
        result.b *= outPulse;
    }

    return result;
}

FadeObjectParams EntityRenderer::getFadeParams(const render::ScenePointer& scene) const {
    FadeObjectParams fadeParams;
    auto item = scene->getItemSafe(_renderItemID);
    if (item.exist()) {
        auto transitionStage = scene->getStage<render::TransitionStage>();
        if (transitionStage && transitionStage->checkId(item.getTransitionId())) {
            auto& transition = transitionStage->getElement(item.getTransitionId());
            if (transition.paramsBuffer._size == sizeof(FadeObjectParams)) {
                fadeParams = static_cast<gpu::StructBuffer<FadeObjectParams>&>(transition.paramsBuffer).get();
            }
        }
    }

    return fadeParams;
}
