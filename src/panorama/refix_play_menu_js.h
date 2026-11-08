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
        btn.SetPanelEvent('onactivate', function() {
            $.DispatchEvent('PlaySoundEffect', 'UIPanorama.mainmenu_press_GO', 'MOUSE');
            SteamOverlayAPI.OpenURL('refix://server');
        });
        $.Msg('[ReFix LAN] LAN server button injected');
    }

    _Inject();
})();
)js";
    return s_script;
}

} // namespace refix::panorama
