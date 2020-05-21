#include "../include/main.hpp"

#include <map>
#include <vector>

#include "../extern/beatsaber-hook/shared/utils/utils.h"
#include "../extern/beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "../extern/beatsaber-hook/shared/config/config-utils.hpp"
#include "../extern/questui/AssetBundle.hpp"
#include "../extern/questui/questui.hpp"
#include "../extern/questui/unity-helper.hpp"

rapidjson::Document& config_doc = Configuration::config;

static AssetBundle* assetBundle = nullptr;
static Il2CppObject* customUIObject = nullptr;

static Il2CppObject* textSave = nullptr;
static Il2CppObject* toggleLights = nullptr;
static Il2CppObject* toggleWalls = nullptr;
static Il2CppObject* toggleSabers = nullptr;
static Il2CppObject* toggleTrails = nullptr;
static Il2CppObject* toggleNotes = nullptr;
static Il2CppObject* toggleQSabers = nullptr;

bool isInTutorial = false;

float saberAColorHue = 0;
float saberBColorHue = 0;
float environmentColor0Hue = 0;
float environmentColor1Hue = 0;
float obstaclesColorHue = 0;

Color saberAColor;
Color saberBColor;
Color environmentColor0;
Color environmentColor1;
Color obstaclesColor;

Il2CppObject* simpleColorSO0 = nullptr;
Il2CppObject* simpleColorSO1 = nullptr;

std::map<Il2CppObject*, std::vector<Il2CppObject*>> sabersMaterials;
Array<Il2CppObject*>* colorManagers = nullptr;
Array<Il2CppObject*>* lightSwitchEventEffects = nullptr;

Il2CppObject* basicSaberModelControllers[2];

Color ColorFromHSV(float h, float s, float v) {
    h /= 360.0f;
    return CRASH_UNLESS(il2cpp_utils::RunMethod<Color>("UnityEngine", "Color", "HSVToRGB", h, s, v));
}

Color GetLinearColor(Color color) {
    Color linearColor;
    auto* method = CRASH_UNLESS(il2cpp_utils::FindMethod(
        "UnityEngine", "Mathf", "GammaToLinearSpace", il2cpp_functions::defaults->single_class));
    linearColor.r = CRASH_UNLESS(il2cpp_utils::RunMethod<float>(nullptr, method, color.r));
    linearColor.g = CRASH_UNLESS(il2cpp_utils::RunMethod<float>(nullptr, method, color.g));
    linearColor.b = CRASH_UNLESS(il2cpp_utils::RunMethod<float>(nullptr, method, color.b));
    linearColor.a = color.a;
    return linearColor;
}

Array<Il2CppObject*>* GetAllObjectsOfType(std::string_view nameSpace, std::string_view klassName) {
    auto* typ = CRASH_UNLESS(il2cpp_utils::GetSystemType(nameSpace, klassName));
    auto* objects = CRASH_UNLESS(il2cpp_utils::RunMethod<Array<Il2CppObject*>*>(
        "UnityEngine", "Resources", "FindObjectsOfTypeAll", typ));
    return objects;
}

Color GetColorFromManager(Il2CppObject* colorManager, std::string_view fieldName) {
    auto* fieldValue = CRASH_UNLESS(il2cpp_utils::GetFieldValue(colorManager, fieldName.data()));
    return CRASH_UNLESS(il2cpp_utils::GetPropertyValue<Color>(fieldValue, "color"));
}

Il2CppObject* CreateColorSO() {
    auto* tSimpleColorSO = CRASH_UNLESS(il2cpp_utils::GetSystemType("", "SimpleColorSO"));
    return CRASH_UNLESS(il2cpp_utils::RunMethod(
        "UnityEngine", "ScriptableObject", "CreateInstance", tSimpleColorSO));
}

void CacheSaberMaterials(Il2CppObject* saber) {
    std::vector<Il2CppObject*> materials;
    Il2CppClass* shaderClass = il2cpp_utils::GetClassFromName("UnityEngine", "Shader");
    int glowID = CRASH_UNLESS(il2cpp_utils::RunMethod<int>(shaderClass, "PropertyToID", il2cpp_utils::createcsstr("_Glow")));
    int bloomID = CRASH_UNLESS(il2cpp_utils::RunMethod<int>(shaderClass, "PropertyToID", il2cpp_utils::createcsstr("_Bloom")));
    auto* childTransforms = CRASH_UNLESS(il2cpp_utils::RunMethod<Array<Il2CppObject*>*>(
        saber, "GetComponentsInChildren", il2cpp_utils::GetSystemType("UnityEngine", "Transform"), false));
    for (int i = 0; i < childTransforms->Length(); i++) {
        auto* renderers = CRASH_UNLESS(il2cpp_utils::RunMethod<Array<Il2CppObject*>*>(childTransforms->values[i], 
            "GetComponentsInChildren", il2cpp_utils::GetSystemType("UnityEngine", "Renderer"), false));
        for (int j = 0; j < renderers->Length(); j++) {
            auto* sharedMaterials = CRASH_UNLESS(il2cpp_utils::GetPropertyValue<Array<Il2CppObject*>*>(
                renderers->values[j], "sharedMaterials"));
            for (int h = 0; h < sharedMaterials->Length(); h++) {
                Il2CppObject* material = sharedMaterials->values[h];
                bool setColor = false;
                bool hasGlow = il2cpp_utils::RunMethod<bool>(material, "HasProperty", glowID).value_or(false);
                if (hasGlow) {
                    float glowFloat = CRASH_UNLESS(il2cpp_utils::RunMethod<float>(material, "GetFloat", glowID));
                    if (glowFloat > 0) setColor = true;
                }
                if (!setColor) {
                    bool hasBloom = il2cpp_utils::RunMethod<bool>(material, "HasProperty", bloomID).value_or(false);
                    if (hasBloom) {
                        float bloomFloat = CRASH_UNLESS(il2cpp_utils::RunMethod<float>(material, "GetFloat", bloomID));
                        if (bloomFloat > 0) setColor = true;
                    }
                }
                if (setColor) {
                    materials.push_back(material);
                }
            }
        }
    }
    if (materials.size() > 0) sabersMaterials[saber] = materials;
}

void SetSaberColor(Il2CppObject* saber, Color color) {
    if (!sabersMaterials.count(saber)) {
        CacheSaberMaterials(saber);
    }
    for (Il2CppObject* material : sabersMaterials[saber]) {
        il2cpp_utils::RunMethod(material, "SetColor", il2cpp_utils::createcsstr("_Color"), color);
    }
}

MAKE_HOOK_OFFSETLESS(SceneManager_SetActiveScene, bool, int scene) {
    saberAColor = {168.0f / 255.0f, 32.0f / 255.0f, 32.0f / 255.0f};   // RED
    saberBColor = {32.0f / 255.0f, 100.0f / 255.0f, 168.0f / 255.0f};  // BLUE
    return SceneManager_SetActiveScene(scene);
}

MAKE_HOOK_OFFSETLESS(TutorialController_Start, void, Il2CppObject* self) {
    TutorialController_Start(self);
    isInTutorial = true;
}

MAKE_HOOK_OFFSETLESS(TutorialController_OnDestroy, void, Il2CppObject* self) {
    TutorialController_OnDestroy(self);
    isInTutorial = false;
}

MAKE_HOOK_OFFSETLESS(ColorManager_ColorForNoteType, Color, Il2CppObject* self, NoteType type) {
    if (Config.Notes) {
        return type == NoteA ? saberAColor : saberBColor;
    }
    return ColorManager_ColorForNoteType(self, type);
}

MAKE_HOOK_OFFSETLESS(ColorManager_ColorForSaberType, Color, Il2CppObject* self, SaberType type) {
    if (Config.Sabers) {
        return type == SaberA ? saberAColor : saberBColor;
    }
    return ColorManager_ColorForSaberType(self, type);
}

MAKE_HOOK_OFFSETLESS(ColorManager_EffectsColorForSaberType, Color, Il2CppObject* self, SaberType type) {
    if (Config.Sabers) {
        return type == SaberA ? saberAColor : saberBColor;
    }
    return ColorManager_EffectsColorForSaberType(self, type);
}

MAKE_HOOK_OFFSETLESS(ColorManager_GetObstacleEffectColor, Color, Il2CppObject* self) {
    if (Config.Walls) {
        return obstaclesColor;
    }
    return ColorManager_GetObstacleEffectColor(self);
}

MAKE_HOOK_OFFSETLESS(SaberManager_RefreshSabers, void, Il2CppObject* self) {
    colorManagers = GetAllObjectsOfType("", "ColorManager");
    lightSwitchEventEffects = GetAllObjectsOfType("", "LightSwitchEventEffect");
    if (!simpleColorSO0 || !simpleColorSO1) {
        simpleColorSO0 = CreateColorSO();
        simpleColorSO1 = CreateColorSO();
    }
    SaberManager_RefreshSabers(self);
}

MAKE_HOOK_OFFSETLESS(SaberManager_Update, void, Il2CppObject* self) {
    if (colorManagers) {
        if (isInTutorial) {
            if (colorManagers->Length() > 0) {
                Il2CppObject* colorManager = colorManagers->values[colorManagers->Length() - 1];
                saberAColor = GetColorFromManager(colorManager, "_saberAColor");
                saberBColor = GetColorFromManager(colorManager, "_saberBColor");
                environmentColor0 = GetColorFromManager(colorManager, "_environmentColor0");
                environmentColor1 = GetColorFromManager(colorManager, "_environmentColor1");
                obstaclesColor = GetColorFromManager(colorManager, "_obstaclesColor");
            }
        } else {
            saberAColorHue = fmod(saberAColorHue + Config.SaberASpeed, 360);
            saberBColorHue = fmod(saberBColorHue + Config.SaberBSpeed, 360);
            saberAColor = ColorFromHSV(saberAColorHue, 1.0f, 1.0f);
            saberBColor = ColorFromHSV(saberBColorHue, 1.0f, 1.0f);

            environmentColor0Hue = fmod(environmentColor0Hue + Config.LightASpeed, 360);
            environmentColor1Hue = fmod(environmentColor1Hue + Config.LightBSpeed, 360);
            environmentColor0 = ColorFromHSV(environmentColor0Hue, 1.0f, 1.0f);
            environmentColor1 = ColorFromHSV(environmentColor1Hue, 1.0f, 1.0f);

            obstaclesColorHue = fmod(obstaclesColorHue + Config.WallsSpeed, 360);
            obstaclesColor = ColorFromHSV(obstaclesColorHue, 1.0f, 1.0f);

            for (int i = 0; i < colorManagers->Length(); i++) {
                auto* colorsDidChangeEvent = CRASH_UNLESS(il2cpp_utils::GetFieldValue(
                    colorManagers->values[i], "colorsDidChangeEvent"));
                if (colorsDidChangeEvent) il2cpp_utils::RunMethod(colorsDidChangeEvent, "Invoke");
            }

            if (Config.Trails) {
                for (int i = 0; i < 2; i++) {
                    Color saberColor = i == SaberA ? saberAColor : saberBColor;
                    Il2CppObject* basicSaberModelController = basicSaberModelControllers[i];
                    auto* saberWeaponTrail = CRASH_UNLESS(il2cpp_utils::GetFieldValue(
                        basicSaberModelController, "_saberWeaponTrail"));
                    il2cpp_utils::SetPropertyValue(saberWeaponTrail, "color", GetLinearColor(saberColor));
                    auto* light = CRASH_UNLESS(il2cpp_utils::GetFieldValue(basicSaberModelController, "_light"));
                    il2cpp_utils::SetPropertyValue(light, "color", saberColor);
                }
            }
            if (Config.Sabers && Config.QSabers) {
                SetSaberColor(CRASH_UNLESS(il2cpp_utils::GetFieldValue(self, "_leftSaber")), saberAColor);
                SetSaberColor(CRASH_UNLESS(il2cpp_utils::GetFieldValue(self, "_rightSaber")), saberBColor);
            }
            if (Config.Lights) {
                il2cpp_utils::RunMethod(simpleColorSO0, "SetColor", environmentColor0);
                il2cpp_utils::RunMethod(simpleColorSO1, "SetColor", environmentColor1);
                for (int i = 0; i < lightSwitchEventEffects->Length(); i++) {
                    Il2CppObject* lightSwitchEventEffect = lightSwitchEventEffects->values[i];
                    if (lightSwitchEventEffect) {
                        il2cpp_utils::SetFieldValue(lightSwitchEventEffect, "_lightColor0", simpleColorSO0);
                        il2cpp_utils::SetFieldValue(lightSwitchEventEffect, "_lightColor1", simpleColorSO1);
                        il2cpp_utils::SetFieldValue(lightSwitchEventEffect, "_highlightColor0", simpleColorSO0);
                        il2cpp_utils::SetFieldValue(lightSwitchEventEffect, "_highlightColor1", simpleColorSO1);
                    }
                }
            }
        }
    }
    SaberManager_Update(self);
}

MAKE_HOOK_OFFSETLESS(BasicSaberModelController_Init, void, Il2CppObject* self, Il2CppObject* parent, SaberType saberType) {
    basicSaberModelControllers[saberType] = self;
    return BasicSaberModelController_Init(self, parent, saberType);
}

MAKE_HOOK_OFFSETLESS(GameNoteController_Update, void, Il2CppObject* self) {
    if (Config.Notes) {
        auto* disappearingArrowController = CRASH_UNLESS(il2cpp_utils::GetFieldValue(self, "_disappearingArrowController"));
        auto* colorNoteVisuals = CRASH_UNLESS(il2cpp_utils::GetFieldValue(disappearingArrowController, "_colorNoteVisuals"));
        auto* noteController = CRASH_UNLESS(il2cpp_utils::GetFieldValue(colorNoteVisuals, "_noteController"));

        auto* noteData = CRASH_UNLESS(il2cpp_utils::GetFieldValue(noteController, "_noteData"));
        NoteType noteType = CRASH_UNLESS(il2cpp_utils::GetPropertyValue<NoteType>(noteData, "noteType"));

        Color noteColor = noteType == NoteA ? saberAColor : saberBColor;
        il2cpp_utils::SetFieldValue(colorNoteVisuals, "_noteColor", noteColor);

        float arrowGlowIntensity = CRASH_UNLESS(il2cpp_utils::GetFieldValue<float>(colorNoteVisuals, "_arrowGlowIntensity"));
        Color arrowGlowSpriteRendererColor = noteColor;
        arrowGlowSpriteRendererColor.a = arrowGlowIntensity;
        auto* arrowGlowSpriteRenderer = CRASH_UNLESS(il2cpp_utils::GetFieldValue(colorNoteVisuals, "_arrowGlowSpriteRenderer"));
        il2cpp_utils::SetPropertyValue(arrowGlowSpriteRenderer, "color", arrowGlowSpriteRendererColor);
        auto* circleGlowSpriteRenderer = CRASH_UNLESS(il2cpp_utils::GetFieldValue(colorNoteVisuals, "_circleGlowSpriteRenderer"));
        il2cpp_utils::SetPropertyValue(circleGlowSpriteRenderer, "color", noteColor);
        auto* materialPropertyBlockControllers = CRASH_UNLESS(il2cpp_utils::GetFieldValue<Array<Il2CppObject*>*>(
            colorNoteVisuals, "_materialPropertyBlockControllers"));

        for (int i = 0; i < materialPropertyBlockControllers->Length(); i++) {
            Il2CppObject* materialPropertyBlockController = materialPropertyBlockControllers->values[i];
            auto* materialPropertyBlock = CRASH_UNLESS(il2cpp_utils::GetPropertyValue(
                materialPropertyBlockController, "materialPropertyBlock"));
            Color materialPropertyBlockColor = noteColor;
            materialPropertyBlockColor.a = 1.0f;
            il2cpp_utils::RunMethod(materialPropertyBlock, "SetColor", il2cpp_utils::createcsstr("_Color"),
                materialPropertyBlockColor);
            il2cpp_utils::RunMethod(materialPropertyBlockController, "ApplyChanges");
        }
    }
    GameNoteController_Update(self);
}

MAKE_HOOK_OFFSETLESS(ObstacleController_Update, void, Il2CppObject* self) {
    if (Config.Walls) {
        auto* stretchableObstacle = CRASH_UNLESS(il2cpp_utils::GetFieldValue(self, "_stretchableObstacle"));

        auto* obstacleFrame = CRASH_UNLESS(il2cpp_utils::GetFieldValue(stretchableObstacle, "_obstacleFrame"));
        float width = CRASH_UNLESS(il2cpp_utils::GetFieldValue<float>(obstacleFrame, "width"));
        float height = CRASH_UNLESS(il2cpp_utils::GetFieldValue<float>(obstacleFrame, "height"));
        float length = CRASH_UNLESS(il2cpp_utils::GetFieldValue<float>(obstacleFrame, "length"));

        il2cpp_utils::RunMethod(stretchableObstacle, "SetSizeAndColor", width, height, length, obstaclesColor);
    }
    ObstacleController_Update(self);
}

void SaveConfig() {
    log(INFO, "Saving Configuration...");
    config_doc.RemoveAllMembers();
    config_doc.SetObject();
    rapidjson::Document::AllocatorType& allocator = config_doc.GetAllocator();
    config_doc.AddMember("Lights", Config.Lights, allocator);
    config_doc.AddMember("Walls", Config.Walls, allocator);
    config_doc.AddMember("Sabers", Config.Sabers, allocator);
    config_doc.AddMember("Trails", Config.Trails, allocator);
    config_doc.AddMember("Notes", Config.Notes, allocator);
    config_doc.AddMember("QSabers", Config.QSabers, allocator);
    config_doc.AddMember("SaberASpeed", Config.SaberASpeed, allocator);
    config_doc.AddMember("SaberBSpeed", Config.SaberBSpeed, allocator);
    config_doc.AddMember("SabersStartDiff", Config.SabersStartDiff, allocator);
    config_doc.AddMember("WallsSpeed", Config.WallsSpeed, allocator);
    Configuration::Write();
    log(INFO, "Saved Configuration!");
}

void TextSaveClear() {
    sleep(1);
    il2cpp_utils::SetPropertyValue(textSave, "enabled", false);
}

void ButtonSaveOnClick(Il2CppObject* button) {
    Config.Lights = UnityHelper::GetToggleIsOn(toggleLights);
    Config.Walls = UnityHelper::GetToggleIsOn(toggleWalls);
    Config.Sabers = UnityHelper::GetToggleIsOn(toggleSabers);
    Config.Trails = UnityHelper::GetToggleIsOn(toggleTrails);
    Config.Notes = UnityHelper::GetToggleIsOn(toggleNotes);
    Config.QSabers = UnityHelper::GetToggleIsOn(toggleQSabers);
    SaveConfig();
    il2cpp_utils::SetPropertyValue(textSave, "enabled", true);
    std::thread textSaveClearThread(TextSaveClear);
    textSaveClearThread.detach();
}

void OnLoadAssetComplete(Asset* asset) {
    customUIObject = CRASH_UNLESS(il2cpp_utils::RunMethod("UnityEngine", "Object", "Instantiate", asset));
    UnityHelper::SetParent(customUIObject, QuestUI::GetQuestUIModInfo()->Panel);

    UnityHelper::AddButtonOnClick(QuestUI::GetQuestUIInfo()->ButtonBinder, customUIObject, "ButtonSave",
        (UnityHelper::ButtonOnClickFunction*)ButtonSaveOnClick);

    textSave = UnityHelper::GetComponentInChildren(
        customUIObject, il2cpp_utils::GetSystemType("TMPro", "TextMeshProUGUI"), "TextSave");
    il2cpp_utils::SetPropertyValue(textSave, "enabled", false);
    il2cpp_utils::SetPropertyValue(textSave, "text", il2cpp_utils::createcsstr("Saved Configuration!"));

    auto* tToggle = CRASH_UNLESS(il2cpp_utils::GetSystemType("UnityEngine.UI", "Toggle"));
    toggleLights = UnityHelper::GetComponentInChildren(customUIObject, tToggle, "ToggleLights");
    toggleWalls = UnityHelper::GetComponentInChildren(customUIObject, tToggle, "ToggleWalls");
    toggleSabers = UnityHelper::GetComponentInChildren(customUIObject, tToggle, "ToggleSabers");
    toggleTrails = UnityHelper::GetComponentInChildren(customUIObject, tToggle, "ToggleTrails");
    toggleNotes = UnityHelper::GetComponentInChildren(customUIObject, tToggle, "ToggleNotes");
    toggleQSabers = UnityHelper::GetComponentInChildren(customUIObject, tToggle, "ToggleQSabers");

    UnityHelper::SetToggleIsOn(toggleLights, Config.Lights);
    UnityHelper::SetToggleIsOn(toggleWalls, Config.Walls);
    UnityHelper::SetToggleIsOn(toggleSabers, Config.Sabers);
    UnityHelper::SetToggleIsOn(toggleTrails, Config.Trails);
    UnityHelper::SetToggleIsOn(toggleNotes, Config.Notes);
    UnityHelper::SetToggleIsOn(toggleQSabers, Config.QSabers);
}

void OnLoadAssetBundleComplete(AssetBundle* assetBundleArg) {
    assetBundle = assetBundleArg;
    assetBundle->LoadAssetAsync("_customasset", OnLoadAssetComplete);
}

void QuestUIOnInitialized() {
    if (!assetBundle) {
        AssetBundle::LoadFromFileAsync("/sdcard/Android/data/com.beatgames.beatsaber/files/uis/rainbowmodUI.qui",
            OnLoadAssetBundleComplete);
    } else {
        OnLoadAssetBundleComplete(assetBundle);
    }
}

bool LoadConfig() {
    log(INFO, "Loading Configuration...");
    Configuration::Load();
    bool foundEverything = true;
    if (config_doc.HasMember("Lights") && config_doc["Lights"].IsBool()) {
        Config.Lights = config_doc["Lights"].GetBool();
    } else foundEverything = false;

    if (config_doc.HasMember("Walls") && config_doc["Walls"].IsBool()) {
        Config.Walls = config_doc["Walls"].GetBool();
    } else foundEverything = false;

    if (config_doc.HasMember("Sabers") && config_doc["Sabers"].IsBool()) {
        Config.Sabers = config_doc["Sabers"].GetBool();
    } else foundEverything = false;

    if (config_doc.HasMember("Trails") && config_doc["Trails"].IsBool()) {
        Config.Trails = config_doc["Trails"].GetBool();
    } else foundEverything = false;

    if (config_doc.HasMember("Notes") && config_doc["Notes"].IsBool()) {
        Config.Notes = config_doc["Notes"].GetBool();
    } else foundEverything = false;

    if (config_doc.HasMember("QSabers") && config_doc["QSabers"].IsBool()) {
        Config.QSabers = config_doc["QSabers"].GetBool();
    } else foundEverything = false;

    if (config_doc.HasMember("SaberASpeed") && config_doc["SaberASpeed"].IsDouble()) {
        Config.SaberASpeed = config_doc["SaberASpeed"].GetDouble();
    } else foundEverything = false;

    if (config_doc.HasMember("SaberBSpeed") && config_doc["SaberBSpeed"].IsDouble()) {
        Config.SaberBSpeed = config_doc["SaberBSpeed"].GetDouble();
    } else foundEverything = false;

    if (config_doc.HasMember("SabersStartDiff") && config_doc["SabersStartDiff"].IsDouble()) {
        Config.SabersStartDiff = config_doc["SabersStartDiff"].GetDouble();
    } else foundEverything = false;

    if (config_doc.HasMember("WallsSpeed") && config_doc["WallsSpeed"].IsDouble()) {
        Config.WallsSpeed = config_doc["WallsSpeed"].GetDouble();
    } else foundEverything = false;

    if (foundEverything) {
        log(INFO, "Loaded Configuration!");
    }
    return foundEverything;
}

extern "C" void load() {
    if (!LoadConfig()) SaveConfig();
    saberBColorHue = Config.SabersStartDiff;
    environmentColor1Hue = Config.LightsStartDiff;
    log(INFO, "Starting RainbowMod installation...");
    il2cpp_functions::Init();
    QuestUI::Initialize("Rainbow Mod", QuestUIOnInitialized);

    INSTALL_HOOK_OFFSETLESS(SceneManager_SetActiveScene,
        il2cpp_utils::FindMethodUnsafe("UnityEngine.SceneManagement", "SceneManager", "SetActiveScene", 1));

    INSTALL_HOOK_OFFSETLESS(TutorialController_Start, il2cpp_utils::FindMethod("", "TutorialController", "Start"));
    INSTALL_HOOK_OFFSETLESS(TutorialController_OnDestroy, il2cpp_utils::FindMethod("", "TutorialController", "OnDestroy"));

    auto* cColorMgr = CRASH_UNLESS(il2cpp_utils::GetClassFromName("", "ColorManager"));
    INSTALL_HOOK_OFFSETLESS(ColorManager_ColorForNoteType, il2cpp_utils::FindMethodUnsafe(cColorMgr, "ColorForNoteType", 1));
    INSTALL_HOOK_OFFSETLESS(ColorManager_ColorForSaberType, il2cpp_utils::FindMethodUnsafe(cColorMgr, "ColorForSaberType", 1));
    INSTALL_HOOK_OFFSETLESS(ColorManager_GetObstacleEffectColor, il2cpp_utils::FindMethod(cColorMgr, "GetObstacleEffectColor"));
    INSTALL_HOOK_OFFSETLESS(
        ColorManager_EffectsColorForSaberType, il2cpp_utils::FindMethodUnsafe(cColorMgr, "EffectsColorForSaberType", 1));

    INSTALL_HOOK_OFFSETLESS(SaberManager_RefreshSabers, il2cpp_utils::FindMethod("", "SaberManager", "RefreshSabers"));
    INSTALL_HOOK_OFFSETLESS(SaberManager_Update, il2cpp_utils::FindMethod("", "SaberManager", "Update"));

    INSTALL_HOOK_OFFSETLESS(
        BasicSaberModelController_Init, il2cpp_utils::FindMethodUnsafe("", "BasicSaberModelController", "Init", 2));
    INSTALL_HOOK_OFFSETLESS(GameNoteController_Update, il2cpp_utils::FindMethod("", "GameNoteController", "Update"));
    INSTALL_HOOK_OFFSETLESS(ObstacleController_Update, il2cpp_utils::FindMethod("", "ObstacleController", "Update"));

    log(INFO, "Successfully installed RainbowMod!");
}
