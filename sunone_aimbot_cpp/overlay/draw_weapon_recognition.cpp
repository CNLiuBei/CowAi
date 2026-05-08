// draw_weapon_recognition.cpp
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include "imgui/imgui.h"
#include "sunone_aimbot_cpp.h"
#include "overlay.h"
#include "weapon/recoil_controller.h"
#include "weapon/recoil_data.h"
#include <string>
#include <vector>
#include <algorithm>

extern WeaponRecognizer* globalWeaponRecognizer;
extern RecoilController* globalRecoilController;

static void DrawWeaponInfoBlock(const char* label, const WeaponInfo& info) {
    ImGui::TextColored(ImVec4(0.39f, 0.40f, 0.95f, 1.0f), "%s", label);

    auto drawRow = [](const char* name, const std::string& val, float conf) {
        ImGui::Text("  %s: %s", name, val.c_str());
        if (conf > 0.0f) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "(%.0f%%)", conf * 100.0f);
        }
    };

    drawRow(u8"武器", info.weaponName, info.weaponConfidence);
    drawRow(u8"瞄准镜", info.scope, info.scopeConfidence);
    drawRow(u8"握把", info.grip, info.gripConfidence);
    drawRow(u8"枪口", info.muzzle, info.muzzleConfidence);
    drawRow(u8"枪托", info.stock, info.stockConfidence);
}

// ===== 弹道编辑器辅助 =====
static int s_selectedWeaponIdx = 0;
static bool s_showSaveMsg = false;
static float s_saveMsgTimer = 0.0f;

static const char* AttachmentKeyToCN(const std::string& key)
{
    if (key == "None")    return u8"无";
    if (key == "stand")   return u8"站立";
    if (key == "down")    return u8"蹲下";
    if (key == "crawl")   return u8"趴下";
    if (key == "xy1")     return u8"消焰器";
    if (key == "xy2")     return u8"消焰器(狙)";
    if (key == "xy3")     return u8"消焰器(冲)";
    if (key == "xx")      return u8"消音器";
    if (key == "xx1")     return u8"消音器(冲)";
    if (key == "bc1")     return u8"补偿器";
    if (key == "bc2")     return u8"补偿器(狙)";
    if (key == "bc3")     return u8"补偿器(冲)";
    if (key == "zt")      return u8"制退器";
    if (key == "angle")   return u8"直角";
    if (key == "red")     return u8"红点";
    if (key == "thumb")   return u8"拇指";
    if (key == "light")   return u8"轻型";
    if (key == "line")    return u8"垂直";
    if (key == "half")    return u8"半截";
    if (key == "reddot")  return u8"红点";
    if (key == "quanxi")  return u8"全息";
    if (key == "x2")      return u8"2倍";
    if (key == "x3")      return u8"3倍";
    if (key == "x4")      return u8"4倍";
    if (key == "x6")      return u8"6倍";
    if (key == "x8")      return u8"8倍";
    if (key == "heavy")   return u8"重型枪托";
    if (key == "normal")  return u8"战术枪托";
    if (key == "pg")      return u8"枪托(PG)";
    if (key == "car")     return u8"载具";
    return key.c_str();
}

static void DrawAttachmentCategory(const char* label,
    std::unordered_map<std::string, AttachmentMultiplier>& cat)
{
    if (!ImGui::TreeNode(label)) return;

    std::vector<std::string> keys;
    for (auto& [k, v] : cat) keys.push_back(k);
    std::sort(keys.begin(), keys.end());

    for (auto& key : keys) {
        auto& am = cat[key];
        const char* displayName = AttachmentKeyToCN(key);
        ImGui::PushID(key.c_str());
        if (am.isSegmented) {
            if (ImGui::TreeNode(displayName)) {
                for (size_t i = 0; i < am.segments.size(); i++) {
                    ImGui::PushID((int)i);
                    float cnt = (float)am.segments[i].count;
                    float mul = (float)am.segments[i].multiplier;
                    ImGui::SetNextItemWidth(150);
                    if (ImGui::InputFloat("##cnt", &cnt, 1.0f, 5.0f, "%.0f"))
                        am.segments[i].count = (int)cnt;
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(150);
                    if (ImGui::InputFloat("##mul", &mul, 0.01f, 0.1f, "%.4f"))
                        am.segments[i].multiplier = (double)mul;
                    ImGui::PopID();
                }
                ImGui::TreePop();
            }
        } else {
            float val = (float)am.fixedValue;
            ImGui::SetNextItemWidth(150);
            if (ImGui::InputFloat(displayName, &val, 0.01f, 0.1f, "%.4f"))
                am.fixedValue = (double)val;
        }
        ImGui::PopID();
    }
    ImGui::TreePop();
}

static void DrawRecoilEditorSection()
{
    auto& db = RecoilDatabase::instance();
    std::lock_guard<std::mutex> lock(db.getMutex());

    if (s_showSaveMsg) {
        s_saveMsgTimer -= ImGui::GetIO().DeltaTime;
        if (s_saveMsgTimer <= 0) s_showSaveMsg = false;
        else ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), u8"保存成功");
    }

    if (ImGui::CollapsingHeader(u8"全局参数")) {
        ImGui::Indent(8);

        float sens = (float)db.getGlobalSensMultiplier();
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputFloat(u8"灵敏度", &sens, 0.01f, 0.1f, "%.4f"))
            db.setGlobalSensMultiplier(sens);

        float vs = (float)db.getGlobalVertSensMultiplier();
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputFloat(u8"垂直灵敏度", &vs, 0.01f, 0.1f, "%.4f"))
            db.setGlobalVertSensMultiplier(vs);

        float breath = (float)db.getGlobalBreathMultiplier();
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputFloat(u8"屏息乘数", &breath, 0.01f, 0.1f, "%.4f"))
            db.setGlobalBreathMultiplier(breath);

        float rc = (float)db.getGlobalRecoilMultiplier();
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputFloat(u8"后坐力乘数", &rc, 0.01f, 0.1f, "%.4f"))
            db.setGlobalRecoilMultiplier(rc);

        float fso = (float)db.getFirstShotOffset();
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputFloat(u8"首发偏移", &fso, 0.01f, 0.1f, "%.4f"))
            db.setFirstShotOffset(fso);

        if (ImGui::TreeNode(u8"倍镜乘数")) {
            auto& scopeRatios = db.scopeRatioValues;
            std::vector<std::string> scopeKeys;
            for (auto& [k, v] : scopeRatios) scopeKeys.push_back(k);
            std::sort(scopeKeys.begin(), scopeKeys.end());

            for (auto& sk : scopeKeys) {
                auto& entry = scopeRatios[sk];
                ImGui::PushID(sk.c_str());
                float sv = (float)db.getScopeMultiplier(sk);
                ImGui::SetNextItemWidth(150);
                if (ImGui::InputFloat(sk.c_str(), &sv, 0.01f, 0.1f, "%.4f")) {
                    db.getScopeMultipliersMut()[sk] = sv;
                    if (entry.isRatio) { entry.fixed = sv; }
                    else { entry.fixed = sv; }
                }
                ImGui::PopID();
            }
            ImGui::TreePop();
        }

        if (ImGui::Button(u8"保存全局配置")) {
            if (db.saveGlobalConfig()) {
                s_showSaveMsg = true; s_saveMsgTimer = 2.0f;
            }
        }

        ImGui::Unindent(8);
    }

    if (ImGui::CollapsingHeader(u8"武器弹道编辑")) {
        ImGui::Indent(8);

        auto names = db.getWeaponNames();
        if (names.empty()) {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), u8"未加载武器");
            ImGui::Unindent(8);
            return;
        }

        if (s_selectedWeaponIdx >= (int)names.size()) s_selectedWeaponIdx = 0;
        const char* preview = names[s_selectedWeaponIdx].c_str();
        ImGui::SetNextItemWidth(150);
        if (ImGui::BeginCombo(u8"武器", preview)) {
            for (int i = 0; i < (int)names.size(); i++) {
                bool sel = (i == s_selectedWeaponIdx);
                if (ImGui::Selectable(names[i].c_str(), sel))
                    s_selectedWeaponIdx = i;
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        std::string wp = names[s_selectedWeaponIdx];

        float baseCoef = (float)db.getBaseCoefficient(wp);
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputFloat(u8"基础系数", &baseCoef, 0.01f, 0.1f, "%.4f"))
            db.setBaseCoefficient(wp, baseCoef);

        int interval = db.getWeaponInterval(wp);
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputInt(u8"射速(ms)", &interval, 1, 5))
            db.setWeaponInterval(wp, interval);

        auto* att = db.getAttachmentTableMut(wp);
        if (att) {
            if (ImGui::TreeNode(u8"配件")) {
                DrawAttachmentCategory(u8"姿态", att->poses);
                DrawAttachmentCategory(u8"枪口", att->muzzles);
                DrawAttachmentCategory(u8"握把", att->grips);
                DrawAttachmentCategory(u8"瞄准镜", att->scopes);
                DrawAttachmentCategory(u8"枪托", att->stocks);
                DrawAttachmentCategory(u8"载具", att->car);
                ImGui::TreePop();
            }
        }

        auto* pat = db.getRecoilPatternMut(wp);
        if (pat && ImGui::TreeNode(u8"弹道模式")) {
            auto& entries = pat->defaultPattern;
            float patH = std::min(300.0f, (float)entries.size() * 24.0f + 10.0f);
            ImGui::BeginChild("##patScroll", ImVec2(0, patH), true);
            for (size_t i = 0; i < entries.size(); i++) {
                ImGui::PushID((int)i);
                float bullet = (float)entries[i].bullet;
                float value = (float)entries[i].value;
                ImGui::SetNextItemWidth(150);
                if (ImGui::InputFloat("##b", &bullet, 1.0f, 5.0f, "%.0f"))
                    entries[i].bullet = (int)bullet;
                ImGui::SameLine();
                ImGui::SetNextItemWidth(150);
                if (ImGui::InputFloat("##v", &value, 0.5f, 5.0f, "%.2f"))
                    entries[i].value = (double)value;
                ImGui::PopID();
            }
            ImGui::EndChild();
            ImGui::TreePop();
        }

        if (ImGui::Button(u8"保存武器")) {
            if (db.saveWeaponFile(wp)) {
                s_showSaveMsg = true; s_saveMsgTimer = 2.0f;
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"保存全部")) {
            if (db.saveAll()) {
                s_showSaveMsg = true; s_saveMsgTimer = 2.0f;
            }
        }

        ImGui::Unindent(8);
    }
}

// ===== 主面板 =====
void draw_weapon_recognition_settings()
{
    ImGui::Text(u8"武器与配件识别");
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Checkbox(u8"启用武器识别", &config.weapon_recognition_enabled))
    {
        config.saveConfig();
        if (globalWeaponRecognizer) {
            if (config.weapon_recognition_enabled) {
                globalWeaponRecognizer->configure(
                    config.weapon_template_path,
                    config.weapon_recognition_threshold,
                    config.weapon_scan_interval_ms);
                globalWeaponRecognizer->start();
            } else {
                globalWeaponRecognizer->stop();
            }
        }
    }

    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text(u8"通过模板匹配识别当前武器和配件");
        ImGui::Text(u8"需要 peijian 目录下的模板图片");
        ImGui::EndTooltip();
    }

    ImGui::Spacing();

    static char pathBuf[256] = {};
    if (pathBuf[0] == '\0') {
        strncpy_s(pathBuf, config.weapon_template_path.c_str(), sizeof(pathBuf) - 1);
    }
    if (ImGui::InputText(u8"模板路径", pathBuf, sizeof(pathBuf), ImGuiInputTextFlags_EnterReturnsTrue)) {
        config.weapon_template_path = pathBuf;
        config.saveConfig();
        if (globalWeaponRecognizer && config.weapon_recognition_enabled) {
            globalWeaponRecognizer->stop();
            globalWeaponRecognizer->configure(
                config.weapon_template_path,
                config.weapon_recognition_threshold,
                config.weapon_scan_interval_ms);
            globalWeaponRecognizer->start();
        }
    }

    if (ImGui::SliderFloat(u8"匹配阈值", &config.weapon_recognition_threshold, 0.5f, 1.0f, "%.2f")) {
        config.saveConfig();
        if (globalWeaponRecognizer) {
            globalWeaponRecognizer->configure(
                config.weapon_template_path,
                config.weapon_recognition_threshold,
                config.weapon_scan_interval_ms);
        }
    }

    if (ImGui::SliderInt(u8"扫描间隔(ms)", &config.weapon_scan_interval_ms, 1, 100)) {
        config.saveConfig();
        if (globalWeaponRecognizer) {
            globalWeaponRecognizer->configure(
                config.weapon_template_path,
                config.weapon_recognition_threshold,
                config.weapon_scan_interval_ms);
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (config.weapon_recognition_enabled && globalWeaponRecognizer && globalWeaponRecognizer->isRunning()) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.4f, 1.0f), u8" 识别运行中");
        ImGui::Spacing();

        const WeaponState& state = globalWeaponRecognizer->getState();

        ImGui::TextColored(ImVec4(0.39f, 0.40f, 0.95f, 1.0f), u8"[状态]");

        std::string pose = state.getPose();
        ImGui::Text(u8"  姿态: %s", pose.c_str());

        bool shooting = state.getIsShooting();
        ImGui::Text(u8"  射击: %s", shooting ? u8"是" : u8"否");

        bool hasBag = state.getHasBag();
        ImGui::Text(u8"  背包: %s", hasBag ? u8"已打开" : u8"未打开");

        bool inCar = state.getInCar();
        ImGui::Text(u8"  载具: %s", inCar ? u8"是" : u8"否");

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (hasBag) {
            WeaponInfo primary = state.getPrimary();
            DrawWeaponInfoBlock(u8"[主武器]", primary);
            ImGui::Spacing();
            WeaponInfo secondary = state.getSecondary();
            DrawWeaponInfoBlock(u8"[副武器]", secondary);
        } else {
            ImGui::TextColored(ImVec4(0.8f, 0.7f, 0.2f, 1.0f), u8"打开背包后识别武器和配件");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.39f, 0.40f, 0.95f, 1.0f), u8"[弹道压枪]");

        auto& db = RecoilDatabase::instance();
        ImGui::Text(u8"  已加载武器: %d", db.getLoadedWeaponCount());

        if (ImGui::Button(u8"重载弹道数据")) {
            if (db.reload()) {
                std::cout << "[RecoilData] Manual reload OK" << std::endl;
            }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::Text(u8"修改 recoil/*.json 后点击重载");
            ImGui::Text(u8"也会每2秒自动检测文件变化");
            ImGui::EndTooltip();
        }

        if (globalRecoilController && globalRecoilController->hasActiveWeapon()) {
            std::string wp = globalRecoilController->getCurrentWeapon();
            int bullet = globalRecoilController->getBulletCount();
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.4f, 1.0f), u8"  武器: %s", wp.c_str());
            ImGui::Text(u8"  子弹计数: %d", bullet);
        } else {
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), u8"  等待武器识别...");
        }
    } else {
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), u8" 识别未运行");
    }

    // 弹道数据编辑区域
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    DrawRecoilEditorSection();
}