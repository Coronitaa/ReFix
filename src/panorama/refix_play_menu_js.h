#pragma once
#include <string>

namespace refix::panorama {

// Injected into mainmenu_play.js: a "LAN server" button next to Start that opens the ReFix overlay's LAN server
// window (refix://server, read in ISteamFriends::ActivateGameOverlayToWebPage). Start itself joins a LAN match.
inline const std::string& GetRefixPlayMenuScript() {
    static const std::string s_script = R"js(
(function() {
    'use strict';
    var s_injectTries = 0;

    function _Inject() {
        var startBtn = $('#StartMatchBtn');
        if (!startBtn) {
            // Builds whose play menu has no #StartMatchBtn keep their own UI; the overlay still hosts and joins.
            if (++s_injectTries < 100) $.Schedule(0.15, _Inject);
            return;
        }
        var bar = startBtn.GetParent().GetParent();
        if (!bar || bar.FindChildTraverse('RefixLanServerBtn')) return;

        var btn = $.CreatePanel('Button', bar, 'RefixLanServerBtn');
        bar.MoveChildBefore(btn, bar.GetChild(0));
        btn.style.backgroundColor = 'rgba(43, 212, 164, 0.16)';
        btn.style.border = '1px solid #2BD4A4';
        btn.style.borderRadius = '3px';
        btn.style.height = '44px';
        btn.style.padding = '0px 24px';
        btn.style.marginRight = '16px';
        btn.style.verticalAlign = 'center';

        var lbl = $.CreatePanel('Label', btn, '');
        lbl.text = '__REFIX_LAN_SERVER_LABEL__';  // replaced with the overlay language at injection
        lbl.style.fontFamily = 'stratum2Font';
        lbl.style.fontWeight = 'bold';
        lbl.style.fontSize = '16px';
        lbl.style.color = '#7FEACB';
        lbl.style.letterSpacing = '1px';
        lbl.style.verticalAlign = 'center';
        lbl.style.horizontalAlign = 'center';

        btn.SetPanelEvent('onmouseover', function() {
            btn.style.backgroundColor = 'rgba(43, 212, 164, 0.32)';
            $.DispatchEvent('PlaySoundEffect', 'UIPanorama.mainmenu_rollover', 'MOUSE');
        });
        btn.SetPanelEvent('onmouseout', function() { btn.style.backgroundColor = 'rgba(43, 212, 164, 0.16)'; });

        function _ResolveSkirmishSubmode(mg) {
            if (!mg) return 'flyingscoutsman';
            var s = mg.toLowerCase();
            if (s.indexOf('armsrace') !== -1 || s.indexOf('gungameprogressive') !== -1) return 'gungameprogressive';
            if (s.indexOf('demolition') !== -1 || s.indexOf('gungametrbomb') !== -1) return 'gungametrbomb';
            if (s.indexOf('flyingscoutsman') !== -1) return 'flyingscoutsman';
            return 'flyingscoutsman';
        }

        function _GetActiveMode() {
            var mode = '';
            var radios = $('#GameModeSelectionRadios');
            if (radios && radios.GetChildCount) {
                var n = radios.GetChildCount();
                for (var i = 0; i < n; ++i) {
                    var c = radios.GetChild(i);
                    if (c && c.checked && c.id) {
                        mode = c.id;
                        break;
                    }
                }
            }
            var mg = '';
            try {
                if (typeof LobbyAPI !== 'undefined' && LobbyAPI.GetSessionSettings) {
                    var st = LobbyAPI.GetSessionSettings();
                    if (st && st.game) {
                        if (!mode && st.game.mode) mode = st.game.mode;
                        if (st.game.mapgroupname) mg = st.game.mapgroupname;
                    }
                }
            } catch (e) {}
            if (mode === 'skirmish') {
                if (!mg) {
                    var list = $('#MapSelectionList');
                    if (list && list.GetChildCount) {
                        for (var j = 0; j < list.GetChildCount(); ++j) {
                            var grp = list.GetChild(j);
                            if (grp && grp.IsVisible && grp.IsVisible() && grp.GetChildCount) {
                                for (var k = 0; k < grp.GetChildCount(); ++k) {
                                    var tile = grp.GetChild(k);
                                    if (tile && tile.checked && tile.GetAttributeString) {
                                        mg = tile.GetAttributeString('mapname', '');
                                        if (mg) break;
                                    }
                                }
                            }
                        }
                    }
                }
                return _ResolveSkirmishSubmode(mg);
            }
            return mode || 'competitive';
        }

        var s_lastMode = '';
        function _WatchModeTabs() {
            var cur = _GetActiveMode();
            if (cur && cur !== s_lastMode) {
                s_lastMode = cur;
                SteamOverlayAPI.OpenURL('refix://mode/' + cur);
            }
            $.Schedule(0.25, _WatchModeTabs);
        }
        _WatchModeTabs();

        btn.SetPanelEvent('onactivate', function() {
            $.DispatchEvent('PlaySoundEffect', 'UIPanorama.mainmenu_press_GO', 'MOUSE');
            var activeMode = _GetActiveMode();
            s_lastMode = activeMode;
            SteamOverlayAPI.OpenURL('refix://server/' + activeMode);
        });
        $.Msg('[ReFix LAN] LAN server button injected');
    }

    _Inject();
})();
)js";
    return s_script;
}

} // namespace refix::panorama
