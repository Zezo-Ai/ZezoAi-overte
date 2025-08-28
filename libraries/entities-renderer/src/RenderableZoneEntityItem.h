//
//  RenderableZoneEntityItem.h
//
//  Created by Clement on 4/22/15.
//  Copyright 2015 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#ifndef hifi_RenderableZoneEntityItem_h
#define hifi_RenderableZoneEntityItem_h

#include <ZoneEntityItem.h>
#include <graphics/Skybox.h>
#include <graphics/Haze.h>
#include <graphics/Bloom.h>
#include <graphics/Stage.h>
#include <LightStage.h>
#include <BackgroundStage.h>
#include <HazeStage.h>
#include <BloomStage.h>
#include <TonemappingStage.h>
#include <AmbientOcclusionStage.h>
#include <NormalMapAttenuationStage.h>
#include <TextureCache.h>
#include "RenderableEntityItem.h"
#include <ComponentMode.h>

namespace render { namespace entities { 

class ZoneEntityRenderer : public TypedEntityRenderer<ZoneEntityItem> {
    using Parent = TypedEntityRenderer<ZoneEntityItem>;
    friend class EntityRenderer;

public:
    ZoneEntityRenderer(const EntityItemPointer& entity);

protected:
    virtual void onRemoveFromSceneTyped(const TypedEntityPointer& entity) override;
    virtual ItemKey getKey() override;
    virtual void doRender(RenderArgs* args) override;
    virtual bool needsRenderUpdateFromTypedEntity(const TypedEntityPointer& entity) const override;
    virtual void doRenderUpdateAsynchronousTyped(const TypedEntityPointer& entity) override;

private:
    void updateKeyZoneItemFromEntity(const TypedEntityPointer& entity);
    void updateKeySunFromEntity(const TypedEntityPointer& entity);
    void updateAmbientLightFromEntity(const TypedEntityPointer& entity);
    void updateHazeFromEntity(const TypedEntityPointer& entity);
    void updateKeyBackgroundFromEntity(const TypedEntityPointer& entity);
    void updateBloomFromEntity(const TypedEntityPointer& entity);
    void updateTonemappingFromEntity(const TypedEntityPointer& entity);
    void updateAmbientOcclusionFromEntity(const TypedEntityPointer& entity);
    void updateNormalMapAttenuationFromEntity(const TypedEntityPointer& entity);
    void updateAmbientMap();
    void updateSkyboxMap();
    void setAmbientURL(const QString& ambientUrl);
    void setSkyboxURL(const QString& skyboxUrl);

    void setSkyboxColor(const glm::vec3& color);
    void setProceduralUserData(const QString& userData);

    graphics::LightPointer editSunLight() { _needSunUpdate = true; return _sunLight; }
    graphics::LightPointer editAmbientLight() { _needAmbientUpdate = true; return _ambientLight; }
    graphics::SunSkyStagePointer editBackground() { _needBackgroundUpdate = true; return _background; }
    graphics::SkyboxPointer editSkybox() { return editBackground()->getSkybox(); }
    graphics::HazePointer editHaze() { _needHazeUpdate = true; return _haze; }
    graphics::BloomPointer editBloom() { _needBloomUpdate = true; return _bloom; }
    graphics::TonemappingPointer editTonemapping() { _needTonemappingUpdate = true; return _tonemapping; }
    graphics::AmbientOcclusionPointer editAmbientOcclusion() { _needAmbientOcclusionUpdate = true; return _ambientOcclusion; }
    graphics::NormalMapAttenuationPointer editNormalMapAttenuation() { _needNormalMapAttenuationUpdate = true; return _normalMapAttenuation; }

    glm::vec3 _lastPosition;
    glm::vec3 _lastDimensions;
    glm::quat _lastRotation;
    bool _lastVisible;

    LightStagePointer _stage;
    const graphics::LightPointer _sunLight { std::make_shared<graphics::Light>() };
    const graphics::LightPointer _ambientLight { std::make_shared<graphics::Light>() };
    const graphics::SunSkyStagePointer _background { std::make_shared<graphics::SunSkyStage>() };
    const graphics::HazePointer _haze { std::make_shared<graphics::Haze>() };
    const graphics::BloomPointer _bloom { std::make_shared<graphics::Bloom>() };
    const graphics::TonemappingPointer _tonemapping { std::make_shared<graphics::Tonemapping>() };
    const graphics::AmbientOcclusionPointer _ambientOcclusion { std::make_shared<graphics::AmbientOcclusion>() };
    const graphics::NormalMapAttenuationPointer _normalMapAttenuation { std::make_shared<graphics::NormalMapAttenuation>() };

    ComponentMode _keyLightMode { COMPONENT_MODE_INHERIT };
    ComponentMode _ambientLightMode { COMPONENT_MODE_INHERIT };
    ComponentMode _skyboxMode { COMPONENT_MODE_INHERIT };
    ComponentMode _hazeMode { COMPONENT_MODE_INHERIT };
    ComponentMode _bloomMode { COMPONENT_MODE_INHERIT };
    ComponentMode _tonemappingMode { COMPONENT_MODE_INHERIT };
    ComponentMode _ambientOcclusionMode { COMPONENT_MODE_INHERIT };
    ComponentMode _normalMapAttenuationMode { COMPONENT_MODE_INHERIT };

    indexed_container::Index _sunIndex { LightStage::INVALID_INDEX };
    indexed_container::Index _ambientIndex { LightStage::INVALID_INDEX };

    BackgroundStagePointer _backgroundStage;
    BackgroundStage::Index _backgroundIndex { BackgroundStage::INVALID_INDEX };

    HazeStagePointer _hazeStage;
    HazeStage::Index _hazeIndex { HazeStage::INVALID_INDEX };

    BloomStagePointer _bloomStage;
    BloomStage::Index _bloomIndex { BloomStage::INVALID_INDEX };

    TonemappingStagePointer _tonemappingStage;
    TonemappingStage::Index _tonemappingIndex { TonemappingStage::INVALID_INDEX };

    AmbientOcclusionStagePointer _ambientOcclusionStage;
    AmbientOcclusionStage::Index _ambientOcclusionIndex { AmbientOcclusionStage::INVALID_INDEX };

    NormalMapAttenuationStagePointer _normalMapAttenuationStage;
    NormalMapAttenuationStage::Index _normalMapAttenuationIndex { NormalMapAttenuationStage::INVALID_INDEX };

    bool _needUpdate { true };
    bool _needSunUpdate { true };
    bool _needAmbientUpdate { true };
    bool _needBackgroundUpdate { true };
    bool _needHazeUpdate { true };
    bool _needBloomUpdate { true };
    bool _needTonemappingUpdate { true };
    bool _needAmbientOcclusionUpdate { true };
    bool _needNormalMapAttenuationUpdate { true };

    KeyLightPropertyGroup _keyLightProperties;
    AmbientLightPropertyGroup _ambientLightProperties;
    HazePropertyGroup _hazeProperties;
    SkyboxPropertyGroup _skyboxProperties;
    BloomPropertyGroup _bloomProperties;
    TonemappingPropertyGroup _tonemappingProperties;
    AmbientOcclusionPropertyGroup _ambientOcclusionProperties;
    NormalMapAttenuationPropertyGroup _normalMapAttenuationProperties;

    // More attributes used for rendering:
    QString _ambientTextureURL;
    NetworkTexturePointer _ambientTexture;
    bool _pendingAmbientTexture { false };

    QString _skyboxTextureURL;
    NetworkTexturePointer _skyboxTexture;
    bool _pendingSkyboxTexture { false };

    QString _proceduralUserData;
};

} } // namespace 

#endif // hifi_RenderableZoneEntityItem_h
