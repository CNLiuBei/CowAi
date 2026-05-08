// draw_recoil_editor.cpp
#define WIN32_LEAN_AND_MEAN
#define _WINSOCKAPI_
#include <winsock2.h>
#include <Windows.h>

#include "imgui/imgui.h"
#include "sunone_aimbot_cpp.h"
#include "overlay.h"
#include "weapon/recoil_data.h"
#include <string>
#include <vector>
#include <algorithm>

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
        float val = (float)am.fixedValue;
        ImGui::SetNextItemWidth(150);
        if (ImGui::InputFloat(displayName, &val, 0.01f, 0.1f, "%.4f"))
            am.fixedValue = (double)val;
        ImGui::PopID();
    }
    ImGui::TreePop();
}

void draw_recoil_editor()
{
    auto& db = RecoilDatabase::instance();
    std::lock_guard<std::mutex> lock(db.getMutex());

    ImGui::Text(u8"弹道数据编辑器");
    ImGui::Separator();
    ImGui::Spacing();

    if (s_showSaveMsg) {
        s_saveMsgTimer -= ImGui::GetIO().DeltaTime;
        if (s_saveMsgTimer <= 0) s_showSaveMsg = false;
        else ImGui::TextColored(ImVec4(0.2f, 0.9f, 0.3f, 1.0f), u8"保存成功");
    }

    if (ImGui::CollapsingHeader(u8"全局参数", ImGuiTreeNodeFlags_DefaultOpen)) {
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

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::CollapsingHeader(u8"武器编辑", ImGuiTreeNodeFlags_DefaultOpen)) {
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