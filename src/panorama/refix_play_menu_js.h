#pragma once
#include <string>

namespace refix::panorama {

inline const std::string& GetRefixPlayMenuScript() {
    static const std::string s_script = R"js(
(function() {
    'use strict';
    $.Msg('[ReFix LAN] Initializing ReFix Play Menu Extension...');

    var s_advSettings = {
        maxrounds: 30,
        roundtime: 1.92,
        buytime: 20,
        startmoney: 800,
        maxmoney: 16000,
        friendlyfire: 0,
        bot_quota: 10,
        bot_difficulty: 1,
        sv_cheats: 0,
        overtime: 0,
        limitteams: 2,
        password: '',
        region: 'LAN'
    };

    function _GetSelectedGameSettings() {
        var map = 'de_dust2';
        var mode = 'competitive';
        try {
            if (typeof LobbyAPI !== 'undefined' && LobbyAPI.GetSessionSettings) {
                var s = LobbyAPI.GetSessionSettings();
                if (s && s.game) {
                    if (s.game.mode) mode = s.game.mode;
                    if (s.game.mapgroupname) {
                        // One entry per ticked map ("mg_de_inferno,mg_de_nuke"): host a random one of them.
                        var groups = s.game.mapgroupname.split(',');
                        var mg = groups[Math.floor(Math.random() * groups.length)];
                        map = mg.indexOf('mg_') === 0 ? mg.substring(3) : mg;
                    }
                }
            }
        } catch(e) {
            $.Msg('[ReFix LAN] Error getting session settings: ' + e);
        }
        return { map: map, mode: mode };
    }

    function _SaveConfigAndHost() {
        var sel = _GetSelectedGameSettings();
        var fullCfg = {
            map: sel.map,
            mode: sel.mode,
            maxrounds: s_advSettings.maxrounds,
            roundtime: s_advSettings.roundtime,
            buytime: s_advSettings.buytime,
            startmoney: s_advSettings.startmoney,
            maxmoney: s_advSettings.maxmoney,
            friendlyfire: s_advSettings.friendlyfire,
            bot_quota: s_advSettings.bot_quota,
            bot_difficulty: s_advSettings.bot_difficulty,
            sv_cheats: s_advSettings.sv_cheats,
            overtime: s_advSettings.overtime,
            limitteams: s_advSettings.limitteams,
            password: s_advSettings.password,
            region: s_advSettings.region
        };

        var cfgStr = JSON.stringify(fullCfg);
        $.Msg('[ReFix LAN] Host config prepared: ' + cfgStr);
        // ReFix reads this URL in ISteamFriends::ActivateGameOverlayToWebPage: the next match start hosts with it.
        SteamOverlayAPI.OpenURL('refix://host/' + encodeURIComponent(cfgStr));

        // Trigger native match start
        $.DispatchEvent('PlaySoundEffect', 'mainmenu_press_GO', 'MOUSE');
        LobbyAPI.StartMatchmaking('', '', '', '');
    }

    function _ShowAdvancedPopup() {
        try {
            _BuildAdvancedPopup();
        } catch (e) {
            $.Msg('[ReFix LAN] Advanced settings popup failed: ' + e);
        }
    }

    function _BuildAdvancedPopup() {
        // Hang the modal off the main menu root: inside the play panel it lands below the visible area.
        var mainPanel = $.GetContextPanel();
        while (mainPanel.GetParent()) mainPanel = mainPanel.GetParent();
        var popupId = 'RefixAdvConfigModal';
        var existing = mainPanel.FindChildTraverse(popupId);
        if (existing) {
            existing.visible = true;
            return;
        }
        $.Msg('[ReFix LAN] Building advanced settings popup on ' + mainPanel.id);

        var modal = $.CreatePanel('Panel', mainPanel, popupId);
        modal.style.width = '100%';
        modal.style.height = '100%';
        modal.style.backgroundColor = 'rgba(0, 0, 0, 0.85)';
        modal.style.flowChildren = 'none';
        modal.hittest = true;

        var card = $.CreatePanel('Panel', modal, 'RefixAdvCard');
        card.style.horizontalAlign = 'center';
        card.style.verticalAlign = 'center';
        card.style.width = '640px';
        card.style.height = 'fit-children';
        card.style.backgroundColor = '#161923';
        card.style.border = '1px solid #3A4454';
        card.style.borderRadius = '6px';
        card.style.padding = '24px 32px';
        card.style.flowChildren = 'down';
        card.style.boxShadow = '0px 0px 24px rgba(0, 0, 0, 0.9)';

        // Title
        var title = $.CreatePanel('Label', card, '');
        title.text = 'CONFIGURACIÓN AVANZADA DE PARTIDA LAN';
        title.style.fontFamily = 'stratum2Font';
        title.style.fontSize = '22px';
        title.style.fontWeight = 'bold';
        title.style.color = '#FFFFFF';
        title.style.letterSpacing = '2px';
        title.style.marginBottom = '16px';
        title.style.horizontalAlign = 'center';

        // Helper to add option row
        function _AddOption(labelTxt, defaultVal, onChange) {
            var row = $.CreatePanel('Panel', card, '');
            row.style.width = '100%';
            row.style.height = '36px';
            row.style.flowChildren = 'right';
            row.style.marginBottom = '6px';

            var lbl = $.CreatePanel('Label', row, '');
            lbl.text = labelTxt;
            lbl.style.fontFamily = 'stratum2Font';
            lbl.style.fontSize = '16px';
            lbl.style.color = '#9BA4B5';
            lbl.style.verticalAlign = 'center';
            lbl.style.width = '320px';

            var entry = $.CreatePanel('TextEntry', row, '');
            entry.text = '' + defaultVal;
            entry.style.width = '240px';
            entry.style.height = '30px';
            entry.style.backgroundColor = '#0E1118';
            entry.style.border = '1px solid #2B3342';
            entry.style.color = '#FFFFFF';
            entry.style.fontFamily = 'stratum2Font';
            entry.style.fontSize = '15px';
            entry.style.paddingLeft = '8px';
            entry.style.verticalAlign = 'center';

            entry.SetPanelEvent('ontextentrychange', function() {
                onChange(entry.text);
            });
            return entry;
        }

        _AddOption('Rondas máximas (mp_maxrounds):', s_advSettings.maxrounds, function(v) { s_advSettings.maxrounds = parseInt(v) || 30; });
        _AddOption('Tiempo de ronda en minutos (mp_roundtime):', s_advSettings.roundtime, function(v) { s_advSettings.roundtime = parseFloat(v) || 1.92; });
        _AddOption('Tiempo de compra en seg (mp_buytime):', s_advSettings.buytime, function(v) { s_advSettings.buytime = parseInt(v) || 20; });
        _AddOption('Dinero inicial (mp_startmoney):', s_advSettings.startmoney, function(v) { s_advSettings.startmoney = parseInt(v) || 800; });
        _AddOption('Dinero máximo (mp_maxmoney):', s_advSettings.maxmoney, function(v) { s_advSettings.maxmoney = parseInt(v) || 16000; });
        _AddOption('Fuego amigo 0/1 (mp_friendlyfire):', s_advSettings.friendlyfire, function(v) { s_advSettings.friendlyfire = parseInt(v) || 0; });
        _AddOption('Cantidad de bots (bot_quota):', s_advSettings.bot_quota, function(v) { s_advSettings.bot_quota = parseInt(v) || 0; });
        _AddOption('Dificultad bots 0-3 (bot_difficulty):', s_advSettings.bot_difficulty, function(v) { s_advSettings.bot_difficulty = parseInt(v) || 1; });
        _AddOption('Trucos 0/1 (sv_cheats):', s_advSettings.sv_cheats, function(v) { s_advSettings.sv_cheats = parseInt(v) || 0; });
        _AddOption('Overtime 0/1 (mp_overtime_enable):', s_advSettings.overtime, function(v) { s_advSettings.overtime = parseInt(v) || 0; });
        _AddOption('Límite diferencia equipos (mp_limitteams):', s_advSettings.limitteams, function(v) { s_advSettings.limitteams = parseInt(v) || 2; });
        _AddOption('Contraseña del servidor (sv_password):', s_advSettings.password, function(v) { s_advSettings.password = v; });

        // Buttons row
        var btnRow = $.CreatePanel('Panel', card, '');
        btnRow.style.width = '100%';
        btnRow.style.flowChildren = 'right';
        btnRow.style.marginTop = '16px';
        btnRow.style.horizontalAlign = 'right';

        var btnSave = $.CreatePanel('Button', btnRow, 'RefixAdvSaveBtn');
        btnSave.style.backgroundColor = '#2E7D32';
        btnSave.style.border = '1px solid #4CAF50';
        btnSave.style.borderRadius = '3px';
        btnSave.style.padding = '8px 24px';
        btnSave.style.marginRight = '12px';

        var lblSave = $.CreatePanel('Label', btnSave, '');
        lblSave.text = 'GUARDAR';
        lblSave.style.fontFamily = 'stratum2Font';
        lblSave.style.fontWeight = 'bold';
        lblSave.style.fontSize = '16px';
        lblSave.style.color = '#FFFFFF';

        btnSave.SetPanelEvent('onactivate', function() {
            modal.visible = false;
            // Applied when "Host match" is pressed (sent with the rest of the host settings).
            $.Msg('[ReFix LAN] Advanced settings saved: ' + JSON.stringify(s_advSettings));
        });

        var btnClose = $.CreatePanel('Button', btnRow, 'RefixAdvCloseBtn');
        btnClose.style.backgroundColor = '#37474F';
        btnClose.style.border = '1px solid #607D8B';
        btnClose.style.borderRadius = '3px';
        btnClose.style.padding = '8px 24px';

        var lblClose = $.CreatePanel('Label', btnClose, '');
        lblClose.text = 'CERRAR';
        lblClose.style.fontFamily = 'stratum2Font';
        lblClose.style.fontWeight = 'bold';
        lblClose.style.fontSize = '16px';
        lblClose.style.color = '#CFD8DC';

        btnClose.SetPanelEvent('onactivate', function() {
            modal.visible = false;
        });
    }

    var s_injectTries = 0;
    function _InjectRefixLanSection() {
        var startBtn = $('#StartMatchBtn');
        if (!startBtn) {
            // Builds whose play menu has no #StartMatchBtn keep their own UI; the overlay still hosts and joins.
            if (++s_injectTries < 100) $.Schedule(0.15, _InjectRefixLanSection);
            else $.Msg('[ReFix LAN] Play menu layout not recognised; buttons not injected');
            return;
        }

        var actionsBarRight = startBtn.GetParent().GetParent();
        if (!actionsBarRight) {
            $.Schedule(0.15, _InjectRefixLanSection);
            return;
        }

        if (actionsBarRight.FindChildInLayoutFile('RefixLanActions')) {
            return; // Already present
        }

        var container = $.CreatePanel('Panel', actionsBarRight, 'RefixLanActions');
        container.style.flowChildren = 'right';
        container.style.verticalAlign = 'center';
        container.style.marginRight = '16px';
        container.style.height = '48px';

        actionsBarRight.MoveChildBefore(container, actionsBarRight.GetChild(0));

        // 1. Button "Configuración avanzada"
        var btnAdv = $.CreatePanel('Button', container, 'RefixAdvConfigBtn');
        btnAdv.style.backgroundColor = 'rgba(33, 150, 243, 0.18)';
        btnAdv.style.border = '1px solid #2196F3';
        btnAdv.style.borderRadius = '3px';
        btnAdv.style.height = '44px';
        btnAdv.style.padding = '0px 18px';
        btnAdv.style.marginRight = '10px';
        btnAdv.style.verticalAlign = 'center';

        var lblAdv = $.CreatePanel('Label', btnAdv, '');
        lblAdv.text = 'CONFIGURACIÓN AVANZADA';
        lblAdv.style.fontFamily = 'stratum2Font';
        lblAdv.style.fontWeight = 'bold';
        lblAdv.style.fontSize = '16px';
        lblAdv.style.color = '#64B5F6';
        lblAdv.style.letterSpacing = '1px';
        lblAdv.style.verticalAlign = 'center';

        btnAdv.SetPanelEvent('onmouseover', function() {
            btnAdv.style.backgroundColor = 'rgba(33, 150, 243, 0.40)';
            $.DispatchEvent('PlaySoundEffect', 'UIPanorama.mainmenu_rollover', 'MOUSE');
        });
        btnAdv.SetPanelEvent('onmouseout', function() {
            btnAdv.style.backgroundColor = 'rgba(33, 150, 243, 0.18)';
        });
        btnAdv.SetPanelEvent('onactivate', function() {
            _ShowAdvancedPopup();
        });

        // 2. Button "Hostear partida"
        var btnHost = $.CreatePanel('Button', container, 'RefixHostMatchBtn');
        btnHost.style.backgroundColor = 'rgba(76, 175, 80, 0.22)';
        btnHost.style.border = '1px solid #4CAF50';
        btnHost.style.borderRadius = '3px';
        btnHost.style.height = '44px';
        btnHost.style.padding = '0px 22px';
        btnHost.style.verticalAlign = 'center';

        var lblHost = $.CreatePanel('Label', btnHost, '');
        lblHost.text = 'HOSTEAR PARTIDA';
        lblHost.style.fontFamily = 'stratum2Font';
        lblHost.style.fontWeight = 'bold';
        lblHost.style.fontSize = '16px';
        lblHost.style.color = '#81C784';
        lblHost.style.letterSpacing = '1px';
        lblHost.style.verticalAlign = 'center';

        btnHost.SetPanelEvent('onmouseover', function() {
            btnHost.style.backgroundColor = 'rgba(76, 175, 80, 0.45)';
            $.DispatchEvent('PlaySoundEffect', 'UIPanorama.mainmenu_rollover', 'MOUSE');
        });
        btnHost.SetPanelEvent('onmouseout', function() {
            btnHost.style.backgroundColor = 'rgba(76, 175, 80, 0.22)';
        });
        btnHost.SetPanelEvent('onactivate', function() {
            _SaveConfigAndHost();
        });

        $.Msg('[ReFix LAN] ReFix LAN buttons injected successfully!');
    }

    _InjectRefixLanSection();
})();
)js";
    return s_script;
}

} // namespace refix::panorama
