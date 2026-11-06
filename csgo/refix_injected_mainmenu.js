'use strict';

var CharacterAnims = ( function()
{


	function _AddModifiersFromWeaponItemId ( itemId, arrModifiers )
	{
		  		                                             
		  		                                 

		  		                                                                   
		  		                                            

		var weaponName = ItemInfo.GetItemDefinitionName( itemId );
		arrModifiers.push( weaponName );

		var weaponType = InventoryAPI.GetWeaponTypeString( itemId );
		arrModifiers.push( weaponType );
	}

	function _NormalizeTeamName ( team, bShort = false )
	{
		team = String(team).toLowerCase();
		
		switch ( team )
		{
			case '2':
			case 't':
			case 'terrorist':
			case 'team_t':
				
				return bShort? 't' : 'terrorist';
			
			case '3':
			case 'ct':
			case 'counter-terrorist':
			case 'team_ct':
				return 'ct';
			
			default:
				return '';
			
		}
	}

	function _TeamForEquip ( team )
	{
		team = team.toLowerCase();
		
		switch ( team )
		{
			case '2':
			case 't':
			case 'terrorist':
			case 'team_t':
				
				return 't';
			
			case '3':
			case 'ct':
			case 'counter-terrorist':
			case 'team_ct':
				return 'ct';
			
			default:
				return '';
			
		}		

	}
	

	var _PlayAnimsOnPanel = function ( importedSettings )
	{
		  
		                                                       
		                                                   
		  
		
		if ( settings === null ) 
		{
			return;
		}

		var settings = ItemInfo.DeepCopyVanityCharacterSettings( importedSettings );

		if ( !settings.team || settings.team == "" )
			settings.team = 'ct';
		
		settings.team = _NormalizeTeamName( settings.team );

		if ( settings.modelOverride ) {
			settings.model = settings.modelOverride;
			          
			                            
				                                                                        
			 
			          
		} else {
			                                                      
			settings.model = ItemInfo.GetModelPlayer( settings.charItemId );
			                                                               
			if ( !settings.model )
			{
				if ( settings.team == 'ct' )
					settings.model = "models/player/custom_player/legacy/ctm_sas.mdl";
				else
					settings.model = "models/player/custom_player/legacy/tm_phoenix.mdl";
			}
		}

		var wid = settings.weaponItemId;
		
		var playerPanel = settings.panel;
		_CancelScheduledAnim( playerPanel );
		_ResetLastRandomAnimHandle( playerPanel );
		
		playerPanel.ResetAnimation( false );
		playerPanel.SetSceneAngles( 0, 0, 0 );

		if ( settings.manifest )
			playerPanel.SetScene( settings.manifest, settings.model, false );

		playerPanel.SetPlayerModel( settings.model );
		playerPanel.EquipPlayerWithItem( wid );
		playerPanel.EquipPlayerWithItem( settings.glovesItemId );

		playerPanel.ResetActivityModifiers();
		
		playerPanel.ApplyActivityModifier( settings.team );

		if ( !( 'arrModifiers' in settings ) )
		{
			settings.arrModifiers = [];
		}

		_AddModifiersFromWeaponItemId( wid, settings.arrModifiers );

		settings.arrModifiers.forEach( mod => playerPanel.ApplyActivityModifier( mod ) );
		
		if ( !('activity' in settings ) || settings.activity == "" )
		{
			settings.activity = 'ACT_CSGO_UIPLAYER_IDLE';
		}
		
  		                                                  
		
		if ( !( 'immediate' in settings ) || settings.immediate == "" )
		{
			settings.immediate = true;
		}

		playerPanel.PlayActivity( settings.activity, settings.immediate );

		var cam = 0;

		if ( 'cameraPreset' in settings && settings.cameraPreset != "" )
		{
			cam = settings.cameraPreset;
			                                               
		}
		else
		{
			cam = _GetCameraPreset( settings.team, ItemInfo.GetSlot( wid ), ItemInfo.GetItemDefinitionName( wid ) );

			          
			                         
			                                                        
			                                                    
			                                          

			                                                                                
			                    
			 
				                                                                            
				                                                                                                        

				                                                                                      
				                                                                                   
				                                                                      
			 
			          
		}

		playerPanel.SetCameraPreset( Number( cam ), false );
	};

	var _CancelScheduledAnim = function ( playerPanel )
	{
		                                                                                             
		if ( playerPanel.Data().handle )
		{
			$.CancelScheduled( playerPanel.Data().handle );
			playerPanel.Data().handle = null;
		}
	};

	var _ResetLastRandomAnimHandle = function ( playerPanel)
	{
		if ( playerPanel.Data().lastRandomAnim !== -1 ) {
			playerPanel.Data().lastRandomAnim = -1;
		}
	};

	var _ItemHasCharacterAnims = function( team, loadoutSlot, selectedWeapon, itemId )
	{
		                                                                                      
		var hasAnims = _GetAnims( team, loadoutSlot, selectedWeapon, itemId ) !== undefined ? true : false;

  		                                                 
		return hasAnims;
	};

	var _GetValidCharacterModels = function( bUniquePerTeamModelsOnly )
	{

		InventoryAPI.SetInventorySortAndFilters ( 'inv_sort_rarity', false, 'customplayer', '', '' );
		var count = InventoryAPI.GetInventoryCount();
		var itemsList = [];
		var uniqueTracker = {};

		for( var i = 0 ; i < count ; i++ )
		{
			var itemId = InventoryAPI.GetInventoryItemIDByIndex( i );
			
			var modelplayer = ItemInfo.GetModelPlayer( itemId );
			if ( !modelplayer )
				continue;

			var team = ( ItemInfo.GetTeam( itemId ).search( 'Team_T' ) === -1 ) ? 'ct' : 't';
			if ( bUniquePerTeamModelsOnly )
			{	                                           
				if ( uniqueTracker.hasOwnProperty( team + modelplayer ) )
					continue;
				uniqueTracker[ team + modelplayer ] = 1;
			}

			var label = ItemInfo.GetName( itemId );
			var entry = {
				label: label,
				team: team,
				itemId: itemId
			};

			itemsList.push( entry );
		}

		return itemsList;


	};

	                                                                                                    
	                                                                                                      
	                                                               
	                                                                                                    
	var _GetCameraPreset = function ( team, slot, weapon )
	{	
		team = team.toLowerCase();
		slot = slot.toLowerCase();
		weapon = weapon.toLowerCase();

		switch ( team )
		{
			case 'ct':
				switch ( slot )
				{
					case 'melee':
						return 1;
					
					case 'secondary':
					case 'smg':
						switch ( weapon )
						{
							case 'weapon_elite':
								return 1;
							
							case 'weapon_p90':
							case 'weapon_mp9':
							case 'weapon_ump45':
							case 'weapon_bizon':
							case 'weapon_mp5sd':
								return 2;
							
							case 'weapon_mp7':
								return 4;	
							
							default:
								return 0;
						}

					case 'rifle':
						switch ( weapon )
						{
							case 'weapon_famas':
							case 'weapon_aug':
								return 2;
							
							default:
								return 1;
						}

					case 'heavy':
						switch ( weapon )
						{
							case 'weapon_nova':
							case 'weapon_xm1014':
							case 'weapon_mag7':
								return 2;
						}	
						
					default:
						switch ( weapon )
						{
							case 'weapon_flashbang':
							case 'weapon_decoy':
							case 'weapon_smokegrenade':
							case 'weapon_incgrenade':
							case 'weapon_hegrenade':
								return 0;
									
							default:
								return 3;
						}
				}

			case 'terrorist':
				switch ( slot )
				{
					case 'melee':
						return 1;
					
					case 'secondary':
					case 'smg':
						switch ( weapon )
						{
							case 'weapon_elite':
							case 'weapon_mp5sd':
							case 'weapon_mac10':
								return 1;
							
							case 'weapon_ump45':
								return 4;
							
							default:
								return 2;
						}

					case 'heavy':
					case 'rifle':
						return 1;
				
					default:
						switch ( weapon )
						{
							case 'weapon_c4':
							case 'weapon_flashbang':
							case 'weapon_decoy':	
							case 'weapon_smokegrenade':
							case 'weapon_incgrenade':
							case 'weapon_hegrenade':
							case 'weapon_molotov':
								return 0;
						}	
				}
		}
	};

	var _GetAnims = function( team, loadoutSlot, weapon, itemId )
	{
		if ( !loadoutSlot )
		{
			if ( team == 'ct' )
			{
				return {
					cameraPreset: 1,
					intro: 'ct_loadout_knife_walkup',
					idle: 'ct_loadout_knife_idle',
					animsList: [
						'ct_loadout_knife_headtilt',
						'ct_loadout_knife_lookat01',
						'ct_loadout_knife_lookat02',
						'ct_loadout_knife_shrug',
						'ct_loadout_knife_flip01',
						'ct_loadout_knife_slicedice01',
						'ct_loadout_knife_slicedice02',
						'ct_loadout_knife_backflip'
					]
				};
			}
			else
			{
				return {
					cameraPreset: 1,
					intro: 't_loadout_knife_walkup',
					idle: 't_loadout_knife_idle',
					animsList: [
						't_loadout_knife_weightshift01',
						't_loadout_knife_headtilt',
						't_loadout_knife_bladewipe',
						't_loadout_knife_threaten',
						't_loadout_knife_flip_frontandback',
						't_loadout_knife_fancymoves',
						't_loadout_knife_slicedice01',
						't_loadout_knife_slicedice02',
						't_loadout_knife_flipandslice'
					]
				};
			}
		}


		if ( team == 'ct' )
		{
			if ( loadoutSlot.indexOf( 'melee' ) !== -1 )
			{
				              
				var defName = InventoryAPI.GetItemDefinitionName( itemId );
				if ( defName.indexOf( 'push' ) !== -1 )
				{
					return {
						cameraPreset: 1,
						intro: 'ct_loadout_push_walkup',
						idle: 'ct_loadout_push_idle',
						animsList: [
							'ct_loadout_push_lookat01',
							'ct_loadout_push_lookat02',
							'ct_loadout_push_shrug',
							'ct_loadout_push_lookat03',
							'ct_loadout_push_raise'
						]
					};
				}
				                
				defName = InventoryAPI.GetItemDefinitionName( itemId );
				if ( defName.indexOf( 'karambit' ) !== -1 )
				{
					return {
						cameraPreset: 1,
						intro: 'ct_loadout_knife_walkup',
						idle: 'ct_loadout_knife_idle',
						animsList: [
							'ct_loadout_knife_headtilt',
							'ct_loadout_knife_lookat01',
							'ct_loadout_knife_lookat02',
							'ct_loadout_knife_shrug',
							'ct_loadout_knife_flip01',
							'ct_loadout_knife_slicedice02'
						]
					};
				}

				          
				return {
					cameraPreset: 1,
					intro: 'ct_loadout_knife_walkup',
					idle: 'ct_loadout_knife_idle',
					animsList: [
						'ct_loadout_knife_headtilt',
						'ct_loadout_knife_lookat01',
						'ct_loadout_knife_lookat02',
						'ct_loadout_knife_shrug',
						'ct_loadout_knife_flip01',
						'ct_loadout_knife_slicedice01',
						'ct_loadout_knife_slicedice02',
						'ct_loadout_knife_backflip'
					]
				};
			}
			           
			if ( loadoutSlot.indexOf( 'secondary' ) !== -1 || loadoutSlot.indexOf( 'smg' ) !== -1 )
			{
				               
				if ( weapon.indexOf( 'elite' ) !== -1 )
				{
					return {
						cameraPreset: 1,
						intro: 'ct_loadout_dual_walkup',
						idle: 'ct_loadout_dual_idle',
						animsList: [
							'ct_loadout_dual_lookat01',
							'ct_loadout_dual_lookat02',
							'ct_loadout_dual_shrug',
							'ct_loadout_dual_lookat03',
							'ct_loadout_dual_raise'
						]
					};
				}
				      
				if ( weapon.indexOf( 'p90' ) !== -1 )
				{
					return {
						cameraPreset: 2,
						intro: 'ct_loadout_p90_walkup',
						idle: 'ct_loadout_p90_idle',
						animsList: [
							'ct_loadout_p90_weightshift01',
							'ct_loadout_p90_lookbehind01',
							'ct_loadout_p90_lookatwatch',
							'ct_loadout_p90_lookbehind02'
						]
					};
				}
				      
				if ( weapon.indexOf( 'mp9' ) !== -1 )
				{
					return {
						cameraPreset: 2,
						intro: 'ct_loadout_mp9_walkup',
						idle: 'ct_loadout_p90_idle',
						animsList: [
							'ct_loadout_p90_weightshift01',
							'ct_loadout_p90_lookbehind01',
							'ct_loadout_p90_lookatwatch',
							'ct_loadout_p90_lookbehind02'
						]
					};
				}
				        
				if ( weapon.indexOf( 'ump45' ) !== -1 )
				{
					return {
						cameraPreset: 2,
						intro: 'ct_loadout_ump45_walkup',
						idle: 'ct_loadout_p90_idle',
						animsList: [
							'ct_loadout_p90_weightshift01',
							'ct_loadout_p90_lookbehind01',
							'ct_loadout_p90_lookatwatch',
							'ct_loadout_p90_lookbehind02'
						]
					};
				}
				        
				if ( weapon.indexOf( 'bizon' ) !== -1 )
				{
					return {
						cameraPreset: 2,
						intro: 'ct_loadout_p90_walkup',
						idle: 'ct_loadout_p90_idle',
						animsList: [
							'ct_loadout_p90_weightshift01',
							'ct_loadout_p90_lookbehind01',
							'ct_loadout_p90_lookatwatch',
							'ct_loadout_p90_lookbehind02'
						]
					};
				}
				      
				if ( weapon.indexOf( 'mp7' ) !== -1 )
				{
					return {
						cameraPreset: 4,
						intro: 'ct_loadout_mp7_walkup',
						idle: 'ct_loadout_mp7_idle',
						animsList: [
							'ct_loadout_mp7_weightshift',
							'ct_loadout_mp7_lookscreenLshakehand',
							'ct_loadout_mp7_lookbehindscreenL',
							'ct_loadout_mp7_lookatgun',
							'ct_loadout_mp7_scanscreenL'
						]
					};
				}
				        
				if ( weapon.indexOf( 'mp5sd' ) !== -1 )
				{
					return {
						cameraPreset: 2,
						intro: 'ct_loadout_mp5sd_walkup',
						idle: 'ct_loadout_p90_idle',
						animsList: [
							'ct_loadout_p90_weightshift01',
							'ct_loadout_p90_lookbehind01',
							'ct_loadout_p90_lookatwatch',
							'ct_loadout_p90_lookbehind02'
						]
					};
				}
				          
				return {
					cameraPreset: 0,
					intro: 'ct_loadout_pistol01_walkup',
					idle: 'ct_loadout_pistol01_idle',
					animsList: [
						'ct_loadout_pistol01_lookscreenLshakehand',
						'ct_loadout_pistol01_weightshift',
						'ct_loadout_pistol01_lookbehindscreenL',
						'ct_loadout_pistol01_lookatgun',
						'ct_loadout_pistol01_scanscreenL'
					]
				};
			}
			
			if ( loadoutSlot.indexOf( 'rifle' ) !== -1 )
			{
				if ( weapon.indexOf( 'famas' ) !== -1 )
				{
					  					                 
					return {
						cameraPreset: 2,
						intro: 'ct_loadout_famas_walkup',
						idle: 'ct_loadout_famas_idle',
						animsList: [
							'ct_loadout_famas_weightshift01',
							'ct_loadout_famas_lookbehind01',
							'ct_loadout_famas_lookatwatch',
							'ct_loadout_famas_lookbehind02'
						]
					};
					
				}
				
				if ( weapon.indexOf( 'm4a1' ) !== -1 || weapon.indexOf( 'm4a1_silencer' ) !== -1 )
				{
					return {
						cameraPreset: 1,
						intro: 'ct_loadout_rifle_walkup_handrepo_m4',
						idle: 'ct_loadout_rifle_idle_handrepo_m4',
						animsList: [
							'ct_loadout_rifle_lookat_handrepo_m4',
							'ct_loadout_rifle_shouldershrug_handrepo_m4',
							'ct_loadout_rifle_weightshift_handrepo_m4'
						]
					};
				}
				if ( weapon.indexOf( 'aug' ) !== -1 )
				{
					  					               
					return {
						cameraPreset: 2,
						intro: 'ct_loadout_rifle_walkup_handrepo_aug',
						idle: 'ct_loadout_rifle_idle_handrepo_aug',
						animsList: [
							'ct_loadout_rifle_weightshift01_handrepo_aug',
							'ct_loadout_rifle_lookbehind01_handrepo_aug',
							'ct_loadout_rifle_lookatwatch_handrepo_aug',
							'ct_loadout_rifle_lookbehind02_handrepo_aug'
						]
					};
					
				}
				if ( weapon.indexOf( 'ssg08' ) !== -1 )
				{
					  					                 
					return {
						cameraPreset: 1,
						intro: 'ct_loadout_rifle_ssg08_walkup',
						idle: 'ct_loadout_rifle_ssg08_idle',
						animsList: [
							'ct_loadout_rifle_ssg08_lookat',
							'ct_loadout_rifle_ssg08_shouldershrug',
							'ct_loadout_rifle_ssg08_weightshift'
						]
					};
					
				}
				if ( weapon.indexOf( 'awp' ) !== -1 )
				{
					  					               
					return {
						cameraPreset: 1,
						intro: 'ct_loadout_rifle_awp_walkup',
						idle: 'ct_loadout_rifle_awp_idle',
						animsList: [
							'ct_loadout_rifle_awp_lookat',
							'ct_loadout_rifle_awp_shouldershrug',
							'ct_loadout_rifle_awp_weightshift'
						]
					};
					
				}
				
				                  
				return {
					cameraPreset: 1,
					intro: 'ct_loadout_rifle_scar_walkup',
					idle: 'ct_loadout_rifle_scar_idle',
					animsList: [
						'ct_loadout_rifle_scar_lookat',
						'ct_loadout_rifle_scar_shouldershrug',
						'ct_loadout_rifle_scar_weightshift'
					]
				};
				
			}

			if ( loadoutSlot.indexOf( 'heavy' ) !== -1 )
			{
				if ( weapon.indexOf( 'nova' ) !== -1 )
				{
					return {
						cameraPreset: 2,
						intro: 'ct_loadout_nova_walkup',
						idle: 'ct_loadout_nova_idle',
						animsList: [
							'ct_loadout_nova_weightshift',
							'ct_loadout_nova_shrug',
							'ct_loadout_nova_lookbehind',
							'ct_loadout_nova_gunlift',
							'ct_loadout_nova_lookat01'
						]
					};
				}
				if ( weapon.indexOf( 'xm1014' ) !== -1 )
				{
					return {
						cameraPreset: 2,
						intro: 'ct_loadout_xm1014_walkup',
						idle: 'ct_loadout_xm1014_idle',
						animsList: [
							'ct_loadout_xm1014_weightshift',
							'ct_loadout_xm1014_shrug',
							'ct_loadout_xm1014_lookbehind',
							'ct_loadout_xm1014_gunlift',
							'ct_loadout_xm1014_lookat01'
						]
					};
				}
				if ( weapon.indexOf( 'mag7' ) !== -1 )
				{
					return {
						cameraPreset: 2,
						intro: 'ct_loadout_mag7_walkup',
						idle: 'ct_loadout_mag7_idle',
						animsList: [
							'ct_loadout_mag7_weightshift01',
							'ct_loadout_mag7_lookbehind01',
							'ct_loadout_mag7_lookatwatch',
							'ct_loadout_mag7_lookbehind02'
						]
					};
					
				}
				if ( weapon.indexOf( 'negev' ) !== -1 )
				{
					return {
						cameraPreset: 3,
						intro: 'ct_loadout_negev_walkup',
						idle: 'ct_loadout_negev_idle',
						animsList: [
							'ct_loadout_negev_lookat01',
							'ct_loadout_negev_lookat02',
							'ct_loadout_negev_lookback',
							'ct_loadout_negev_lift01',
							'ct_loadout_negev_lift02',
							'ct_loadout_negev_weightshift'
							  							                      
						]
					};
				}
				
			if ( loadoutSlot.indexOf( 'flashbang' ) !== -1 || loadoutSlot.indexOf( 'decoy') !== -1 )
			{
				                              
				return {
					cameraPreset: 0,
					intro: 'ct_loadout_frag01_walkup',
					idle: 'ct_loadout_frag01_idle',
					animsList: [
						'ct_loadout_frag01_touch',
						'ct_loadout_frag01_lookat01',
						'ct_loadout_frag01_toss01',
						'ct_loadout_frag01_toss02'
					]
				};
			}
			
			if ( loadoutSlot.indexOf( 'smokegrenade' ) !== -1 || loadoutSlot.indexOf( 'incgrenade') !== -1 || loadoutSlot.indexOf( 'hegrenade') !== -1 )
			{
				                                  
				return {
					cameraPreset: 0,
					intro: 'ct_loadout_frag02_walkup',
					idle: 'ct_loadout_frag02_idle',
					animsList: [
						'ct_loadout_frag02_touch',
						'ct_loadout_frag02_lookat01',
						'ct_loadout_frag02_toss01',
						'ct_loadout_frag02_toss02'
					]
				};
			}
			
		
				          
				return {
					cameraPreset: 3,
					intro: 'ct_loadout_heavy_walkup',
					idle: 'ct_loadout_heavy_idle',
					animsList: [
						'ct_loadout_heavy_lookat01',
						'ct_loadout_heavy_lookat02',
						'ct_loadout_heavy_lookback',
						'ct_loadout_heavy_lift01',
						'ct_loadout_heavy_lift02',
						'ct_loadout_heavy_weightshift'
						  						                      
					]
				};
			}
		}
		else
		{
			          
			if ( loadoutSlot.indexOf( 'c4' ) !== -1 )
			{
				    
				return {
					cameraPreset: 0,
					intro: 't_loadout_c4_walkup',
					idle: 't_loadout_c4_idle',
					animsList: [
						't_loadout_c4_lookat01',
						't_loadout_c4_catch',
						't_loadout_c4_bobble',
						't_loadout_c4_lookat02'
					]
				};
			}

			if ( loadoutSlot.indexOf( 'flashbang' ) !== -1 || loadoutSlot.indexOf( 'decoy' ) !== -1 )
			{
				                              
				return {
					cameraPreset: 0,
					intro: 't_loadout_frag_walkup',
					idle: 't_loadout_frag_idle',
					animsList: [
						't_loadout_frag_lookat',
						't_loadout_frag_toss01',
						't_loadout_frag_toss02',
						't_loadout_frag_weightshift'
					]
				};
			}
			
			if ( loadoutSlot.indexOf( 'smokegrenade' ) !== -1 || loadoutSlot.indexOf( 'incgrenade' ) !== -1 || loadoutSlot.indexOf( 'hegrenade' ) !== -1 )
			{
				                                  
				return {
					cameraPreset: 0,
					intro: 't_loadout_molotov_walkup',
					idle: 't_loadout_molotov_idle',
					animsList: [
						't_loadout_molotov_lookat',
						't_loadout_molotov_toss01',
						't_loadout_smoke_incen_toss02',
						't_loadout_molotov_weightshift'
					]
				};
			}
			
			if ( loadoutSlot.indexOf( 'molotov' ) !== -1 )
			{
				              
				return {
					cameraPreset: 0,
					intro: 't_loadout_molotov_walkup',
					idle: 't_loadout_molotov_idle',
					animsList: [
						't_loadout_molotov_lookat',
						't_loadout_molotov_toss01',
						't_loadout_molotov_toss02',
						't_loadout_molotov_weightshift'
					]
				};
			}
			
			if ( loadoutSlot.indexOf( 'melee' ) !== -1 )
			{
				             
				var defName = InventoryAPI.GetItemDefinitionName( itemId );
				if ( defName.indexOf( 'push' ) !== -1 )
				{
					return {
						cameraPreset: 1,
						intro: 't_loadout_push_walkup',
						idle: 't_loadout_push_idle',
						animsList: [
							't_loadout_push_bellyrub',
							't_loadout_push_lookat01',
							't_loadout_push_lookat02',
							't_loadout_push_aimknife',
							't_loadout_push_lookat03',
							't_loadout_push_headtilt'
						]
					};
				}
				                
				defName = InventoryAPI.GetItemDefinitionName( itemId );
				if ( defName.indexOf( 'karambit' ) !== -1 )
				{
					return {
						cameraPreset: 1,
						intro: 't_loadout_knife_walkup',
						idle: 't_loadout_knife_idle',
						animsList: [
							't_loadout_knife_weightshift01',
							't_loadout_knife_headtilt',
							't_loadout_knife_threaten',
							't_loadout_knife_flip_frontandback',
							't_loadout_knife_fancymoves',
							't_loadout_knife_slicedice01',
							't_loadout_knife_slicedice02',
							't_loadout_knife_flipandslice'
						]
					};
				}
				          
				return {
					cameraPreset: 1,
					intro: 't_loadout_knife_walkup',
					idle: 't_loadout_knife_idle',
					animsList: [
						't_loadout_knife_weightshift01',
						't_loadout_knife_headtilt',
						't_loadout_knife_bladewipe',
						't_loadout_knife_threaten',
						't_loadout_knife_flip_frontandback',
						't_loadout_knife_fancymoves',
						't_loadout_knife_slicedice01',
						't_loadout_knife_slicedice02',
						't_loadout_knife_flipandslice'
					]
				};
			}
			if ( loadoutSlot.indexOf( 'secondary' ) !== -1 || loadoutSlot.indexOf( 'smg' ) !== -1 )
			{
				if ( weapon.indexOf( 'elite' ) !== -1 )
				{
					return {
						cameraPreset: 1,
						intro: 't_loadout_dual_walkup',
						idle: 't_loadout_dual_idle',
						animsList: [
							't_loadout_dual_bellyrub',
							't_loadout_dual_lookatgun01',
							't_loadout_dual_lookatgun02',
							't_loadout_dual_lookatgun03',
							't_loadout_dual_headtilt'
						]
					};
				}
				
				if ( weapon.indexOf( 'revolver' ) !== -1 )
				{
					return {
						cameraPreset: 2,
						intro: 't_loadout_pistol_walkup',
						idle: 't_loadout_pistol_idle',
						animsList: [
							't_loadout_pistol_weightshift',
							't_loadout_pistol_lookat_pistol',
							't_loadout_pistol_lookat_pistol02',
							't_loadout_pistol_headscratch',
							't_loadout_pistol_buttscratch'
						]
					};
				}
				if ( weapon.indexOf( 'p90' ) !== -1 )
				{
					return {
						cameraPreset: 2,
						intro: 't_loadout_p90_walkup',
						idle: 't_loadout_p90_idle',
						animsList: [
							't_loadout_p90_weightshift',
							't_loadout_p90_lookataround',
							't_loadout_p90_lookatgun',
							't_loadout_p90_lookaround02',
							't_loadout_p90_spray',
							't_loadout_p90_lookaround03'
						]
					};
				}
				if ( weapon.indexOf( 'ump45' ) !== -1 )
				{
					return {
						cameraPreset: 4,
						intro: 't_loadout_ump45_walkup',
						idle: 't_loadout_ump45_idle',
						animsList: [
							't_loadout_ump45_weightshift',
							't_loadout_ump45_lookataround',
							't_loadout_ump45_lookatgun',
							't_loadout_ump45_lookaround02',
							't_loadout_ump45_spray',
							't_loadout_ump45_lookaround03'
						]
					};
				}
				if ( weapon.indexOf( 'mp5sd' ) !== -1 )
				{
					return {
						cameraPreset: 1,
						intro: 't_loadout_shotgun_xm_walkup',
						idle: 't_loadout_shotgun_xm_idle',
						animsList: [
							't_loadout_shotgun_xm_weightshift',
							't_loadout_shotgun_xm_lookat01',
							't_loadout_shotgun_xm_shrug',
							't_loadout_shotgun_xm_headcock',
							't_loadout_shotgun_xm_headgrab',
							't_loadout_shotgun_xm_bellyscratch',
							't_loadout_shotgun_xm_lookback'
						]
					};
				}
				if ( weapon.indexOf( 'mp7' ) !== -1 )
				{
					return {
						cameraPreset: 2,
						intro: 't_loadout_mp7_walkup',
						idle: 't_loadout_mp7_idle',
						animsList: [
							't_loadout_mp7_weightshift',
							't_loadout_mp7_lookataround',
							't_loadout_mp7_lookatgun',
							't_loadout_mp7_lookaround02',
							't_loadout_mp7_spray',
							't_loadout_mp7_lookaround03'
						]
					};
				}
				if ( weapon.indexOf( 'bizon' ) !== -1 )
				{
					return {
						cameraPreset: 2,
						intro: 't_loadout_bizon_walkup',
						idle: 't_loadout_bizon_idle',
						animsList: [
							't_loadout_bizon_weightshift',
							't_loadout_bizon_lookataround',
							't_loadout_bizon_lookatgun',
							't_loadout_bizon_lookaround02',
							't_loadout_bizon_spray',
							't_loadout_bizon_lookaround03'
						]
					};
				}
				         
				if ( weapon.indexOf( 'mac10' ) !== -1 )
				{
					return {
						cameraPreset: 1,
						intro: 't_loadout_shotgun_xm_walkup',
						idle: 't_loadout_shotgun_xm_idle',
						animsList: [
							't_loadout_shotgun_xm_weightshift',
							't_loadout_shotgun_xm_lookat01',
							't_loadout_shotgun_xm_shrug',
							't_loadout_shotgun_xm_headcock',
							't_loadout_shotgun_xm_headgrab',
							't_loadout_shotgun_xm_bellyscratch',
							't_loadout_shotgun_xm_lookback'
						]
					};
				}
				return {
					cameraPreset: 2,
					intro: 't_loadout_pistol_walkup',
					idle: 't_loadout_pistol_idle',
					animsList: [
						't_loadout_pistol_weightshift',
						't_loadout_pistol_lookat_pistol',
						't_loadout_pistol_slidepull',
						't_loadout_pistol_lookat_pistol02',
						't_loadout_pistol_headscratch',
						't_loadout_pistol_buttscratch'
					]
				};
			}

			if ( loadoutSlot.indexOf( 'heavy' ) !== -1 )
			{
				if ( weapon.indexOf( 'negev' ) !== -1 )
				{
					return {
						cameraPreset: 1,
						intro: 't_loadout_heavy_walkup',
						idle: 't_loadout_heavy_idle',
						animsList: [
							't_loadout_heavy_lookaround01',
							't_loadout_heavy_lookleft',
							't_loadout_heavy_lookaround02',
							't_loadout_heavy_hoist',
							't_loadout_heavy_lookatgun01',
							't_loadout_heavy_lookaround03'
						]
					};
				}
				if ( weapon.indexOf( 'm249' ) !== -1 )
				{
					return {
						cameraPreset: 1,
						intro: 't_loadout_heavy_m249_walkup',
						idle: 't_loadout_heavy_m249_idle',
						animsList: [
							't_loadout_heavy_m249_lookaround01',
							't_loadout_heavy_m249_lookleft',
							't_loadout_heavy_m249_lookaround02',
							't_loadout_heavy_m249_hoist',
							't_loadout_heavy_m249_lookatgun01',
							't_loadout_heavy_m249_lookaround03'
						]
					};
				}
				if ( weapon.indexOf( 'xm1014' ) !== -1 )
				{
					return {
						cameraPreset: 1,
						intro: 't_loadout_shotgun_xm_walkup',
						idle: 't_loadout_shotgun_xm_idle',
						animsList: [
							't_loadout_shotgun_xm_weightshift',
							't_loadout_shotgun_xm_lookat01',
							't_loadout_shotgun_xm_shrug',
							't_loadout_shotgun_xm_headcock',
							't_loadout_shotgun_xm_headgrab',
							't_loadout_shotgun_xm_bellyscratch',
							't_loadout_shotgun_xm_lookback'
						]
					};
				}
				                           
				return {
					cameraPreset: 1,
					intro: 't_loadout_shotgun_walkup',
					idle: 't_loadout_shotgun_idle',
					animsList: [
						't_loadout_shotgun_weightshift',
						't_loadout_shotgun_lookat01',
						't_loadout_shotgun_shrug',
						't_loadout_shotgun_headcock',
						't_loadout_shotgun_headgrab',
						't_loadout_shotgun_bellyscratch',
						't_loadout_shotgun_lookback'
					]
				};
			}
			if ( loadoutSlot.indexOf( 'rifle' ) !== -1 )
			{
				if ( weapon.indexOf( 'awp' ) !== -1 )
				{
					return {
						cameraPreset: 1,
						intro: 't_loadout_rifle02_walkup_awp',
						idle: 't_loadout_rifle02_idle_awp',
						animsList: [
							't_loadout_rifle02_weightshift_awp',
							't_loadout_rifle02_lookback_awp',
							't_loadout_rifle02_lookaround_awp',
							't_loadout_rifle02_lookback02_awp',
							't_loadout_rifle02_lookbatgun_awp'
						]
					};
				}
				
				if ( weapon.indexOf( 'galilar' ) !== -1 )
				{
					return {
						cameraPreset: 1,
						intro: 't_loadout_rifle02_walkup_galil',
						idle: 't_loadout_rifle02_idle_galil',
						animsList: [
							't_loadout_rifle02_weightshift_galil',
							't_loadout_rifle02_lookback_galil',
							't_loadout_rifle02_lookaround_galil',
							't_loadout_rifle02_lookback02_galil',
							't_loadout_rifle02_lookbatgun_galil'
						]
					};
				}
				
				if ( weapon.indexOf( 'g3sg1' ) !== -1 ) 
				{
					return {
						cameraPreset: 1,
						intro: 't_loadout_rifle02_walkup_g3sg',
						idle: 't_loadout_rifle02_idle_g3sg',
						animsList: [
							't_loadout_rifle02_weightshift_g3sg',
							't_loadout_rifle02_lookback_g3sg',
							't_loadout_rifle02_lookaround_g3sg',
							't_loadout_rifle02_lookback02_g3sg',
							't_loadout_rifle02_lookbatgun_g3sg'
						]
					};
				}
				
				          
				return {
					cameraPreset: 1,
					intro: 't_loadout_rifle02_walkup',
					idle: 't_loadout_rifle02_idle',
					animsList: [
						't_loadout_rifle02_weightshift',
						't_loadout_rifle02_lookback',
						't_loadout_rifle02_lookaround',
						't_loadout_rifle02_lookback02',
						't_loadout_rifle02_lookatgun'
						  						                             
					]
				};
			}
		}
	};

	return {
		PlayAnimsOnPanel			: _PlayAnimsOnPanel,
		CancelScheduledAnim			: _CancelScheduledAnim,
		ItemHasCharacterAnims		: _ItemHasCharacterAnims,
		GetAnims					: _GetAnims,
		GetValidCharacterModels		: _GetValidCharacterModels,
		NormalizeTeamName			: _NormalizeTeamName
	};
})();
;'use strict';

var ItemInfo = ( function() {
	var _GetRarityColor = function( id )
	{
		return InventoryAPI.GetItemRarityColor( id );
	};

                                    
	var _GetFormattedName = function ( id )
	{
	    var strName = _GetName( id );

	    if ( InventoryAPI.HasCustomName( id ) )
	    {
	        return new CFormattedText( '#CSGO_ItemName_Custom', { item_name: strName } );
	    }
	    else
	    {
	                                                                                                   
	        var splitLoc = strName.indexOf('|');
            
	        if ( splitLoc >= 0 )
	        {
	            var strWeaponName = strName.substr( 0, splitLoc ).trimRight();                                          
	            var strPaintName  = strName.substr( splitLoc + 1 ).trimLeft();                                      

	            return new CFormattedText( '#CSGO_ItemName_Painted', { item_name: strWeaponName, paintkit_name: strPaintName } );
	        }

	        return new CFormattedText( '#CSGO_ItemName_Base', { item_name: strName } );
	    }
	};

	var _AddItemToShuffle = function( id, team )
	{
		return LoadoutAPI.AddItemToShuffle( id, team );
	};

	var _RemoveItemFromShuffle = function( id, team )
	{
		return LoadoutAPI.RemoveItemFromShuffle( id, team );
	};

	var _IsItemInShuffleForTeam = function( id, team )
	{
		return LoadoutAPI.IsItemInShuffleForTeam( id, team );
	};

	var _ClearShuffle = function( id, team )
	{
		return LoadoutAPI.ClearShuffle ( id, team );
	};

	var _SetShuffleEnabled = function ( id, team, enable )
	{
		return LoadoutAPI.SetShuffleEnabled( id, team, enable );
	};

	var _IsShuffleEnabled = function ( id, team )
	{
		return LoadoutAPI.IsShuffleEnabled( id, team );
	};

	var _IsShuffleAllowed = function ( id )
	{
		return LoadoutAPI.IsShuffleAllowed( id );
	};

	var _CountItemsInInventoryForShuffleSlot = function ( id, team )
	{
		return LoadoutAPI.CountItemsInInventoryForShuffleSlot( id, team );
	};

	var _EnsureShuffleItemEquipped = function ( itemID, team )
	{
		var equippedItemID = ItemInfo.GetItemIdForItemEquippedInLoadoutSlot( itemID, team );
		if ( !LoadoutAPI.IsItemInShuffleForTeam( equippedItemID, team ) )
		{
			LoadoutAPI.ShuffleEquipmentInSlot( equippedItemID, team );
		}
	};

	var _GetName = function( id )
	{
		return InventoryAPI.GetItemName( id );
	};

	var _IsEquippedForCT = function( id )
	{
		return InventoryAPI.IsEquipped( id, 'ct' );
	};

	var _IsEquippedForT = function( id )
	{
		return InventoryAPI.IsEquipped( id, 't' );
	};

	var _IsEquippedForNoTeam = function( id )
	{
		return InventoryAPI.IsEquipped( id, "noteam" );
	};

	var _IsEquipped = function( id, team )
	{
		return InventoryAPI.IsEquipped( id, team );
	};

	var _GetSlot = function( id )
	{
		return InventoryAPI.GetSlot( id );
	};

	var _GetSlotSubPosition = function( id )
	{
		return InventoryAPI.GetSlotSubPosition( id );
	};

	var _IsLoadoutSlotSubPositionAWeapon = function( slotSubPosition )
	{
		return ItemDataAPI.IsLoadoutSlotSubPositionAWeapon( slotSubPosition );
	};	

	var _GetTeam = function( id )
	{
		return InventoryAPI.GetItemTeam( id );
	};

	var _IsSpraySealed = function( id )
	{
		return InventoryAPI.DoesItemMatchDefinitionByName( id, 'spray' );
	};

	var _IsSprayPaint = function( id )
	{
		return InventoryAPI.DoesItemMatchDefinitionByName( id, 'spraypaint' );
	};

	var _IsTradeUpContract = function ( id )
	{
		return InventoryAPI.DoesItemMatchDefinitionByName( id, 'Recipe Trade Up' );
	};

	var _GetSprayTintColor = function( id )
	{
		return InventoryAPI.GetSprayTintColorCode( id );
	};

	var _IsTool = function( id )
	{
		return InventoryAPI.IsTool( id );
	};

	var _GetCapabilitybyIndex = function( id, index )
	{
		return InventoryAPI.GetItemCapabilityByIndex( id, index );
	};

	var _GetCapabilityCount = function( id )
	{
		return InventoryAPI.GetItemCapabilitiesCount( id );
	};

	var _ItemHasCapability = function( id, capName )
	{
		var caps = [];
		var capCount = _GetCapabilityCount( id );

		for ( var i = 0; i < capCount; i++ )
		{
			caps.push( _GetCapabilitybyIndex( id, i ) );
		}

		return caps.includes( capName );
	};

	var _GetChosenActionItemsCount = function( id, capability )
	{
		return InventoryAPI.GetChosenActionItemsCount( id, capability );
	};

	var _GetChosenActionItemIDByIndex = function( id, capability, index )
	{
		return InventoryAPI.GetChosenActionItemIDByIndex( id, capability, index );
	};

	var _GetKeyForCaseInXray = function( caseId )
	{
		var numActionItems = _GetChosenActionItemsCount( caseId, 'decodable' );
		if ( numActionItems > 0 )
		{
			                                                      
			var aKeyIds = [];
			for ( var i = 0; i < numActionItems; i++ )
			{
				aKeyIds.push( _GetChosenActionItemIDByIndex( caseId, 'decodable', i ) );
			}

			aKeyIds.sort( function( a, b ) { return a - b; } );
			return aKeyIds[ 0 ];
		}

		return '';
	};

	var _GetItemsInXray = function()
	{
		InventoryAPI.SetInventorySortAndFilters( 'inv_sort_age', false, 'xraymachine', '', '' );
		var count = InventoryAPI.GetInventoryCount();

		if ( count === 0 )
		{
			return {};
		}

		var xrayCaseId = '';
		var xrayRewardId = '';
		for ( var i = 0; i < count; i++ )
		{
			var id = InventoryAPI.GetInventoryItemIDByIndex( i );

			xrayRewardId = i === 0 ? id : xrayRewardId;
			xrayCaseId = i === 1 ? id : xrayCaseId;
		}

		return { case: xrayCaseId, reward: xrayRewardId };
	};

	var _GetLoadoutWeapons = function( team )
	{
		team =	CharacterAnims.NormalizeTeamName( team, true );

		var list = [];

		var slotStrings = LoadoutAPI.GetLoadoutSlotNames( false );
		var slots = JSON.parse( slotStrings );

		slots.forEach( slot => 
		{
			var weaponItemId = LoadoutAPI.GetItemID( team, slot );

			var bIsWeapon = ItemInfo.IsWeapon( weaponItemId );

			if ( bIsWeapon )
			{
				list.push( weaponItemId );
			}
		} );

		return list;
	}

	var _DeepCopyVanityCharacterSettings = function( inVanityCharacterSettings )
	{
		var modelRenderSettingsOneOffTempCopy =                                                                     
			JSON.parse( JSON.stringify( inVanityCharacterSettings ) );
		modelRenderSettingsOneOffTempCopy.panel = inVanityCharacterSettings.panel;
		return modelRenderSettingsOneOffTempCopy;
	}

	var _PrecacheVanityCharacterSettings = function( inVanityCharacterSettings )
	{
		if ( inVanityCharacterSettings.weaponItemId )
			InventoryAPI.PrecacheCustomMaterials( inVanityCharacterSettings.weaponItemId );
		if ( inVanityCharacterSettings.glovesItemId )
			InventoryAPI.PrecacheCustomMaterials( inVanityCharacterSettings.glovesItemId );
	}

	var _GetOrUpdateVanityCharacterSettings = function( optionalCharacterItemId, optionalState )
	{
		var oSettings = {
			panel: '',
			activity: '',                                                        
			team: '',
			charItemId: '',
			loadoutSlot: '',
			weaponItemId: '',
			glovesItemId: '',
			cameraPreset: '',
			arrModifiers:  [],
		};

		  
		                                              
		  
		if ( optionalCharacterItemId && InventoryAPI.IsValidItemID( optionalCharacterItemId ) )
		{
			var charTeam = ItemInfo.GetTeam( optionalCharacterItemId );
			if ( charTeam.search( 'Team_CT' ) !== -1 )
				oSettings.team = 'ct';
			else if ( charTeam.search( 'Team_T' ) !== -1 )
				oSettings.team = 't';

			if ( oSettings.team )
				oSettings.charItemId = optionalCharacterItemId;
		}

		  
		                                          
		                                                                   
		  
		if ( !oSettings.team )
		{
			oSettings.team = GameInterfaceAPI.GetSettingString( 'ui_vanitysetting_team' );
			if ( oSettings.team !== 'ct' && oSettings.team !== 't' )
			{
				oSettings.team = ( Math.round( Math.random() ) > 0 ) ? 'ct' : 't';
				                                                   
				GameInterfaceAPI.SetSettingString( 'ui_vanitysetting_team', oSettings.team );
			}
		}

		var _fnRollRandomLoadoutSlotAndWeapon = function( strTeam ) {
			var myResult = {
				loadoutSlot: '',
				weaponItemId: ''
			};
			var slots = JSON.parse( LoadoutAPI.GetLoadoutSlotNames( false ) );
			while ( slots.length > 0 )
			{
				var nRandomSlotIndex = Math.floor( Math.random() * slots.length );
				myResult.loadoutSlot = slots.splice( nRandomSlotIndex, 1 )[ 0 ];                                     
				myResult.weaponItemId = LoadoutAPI.GetItemID( strTeam, myResult.loadoutSlot );
				if ( ItemInfo.IsWeapon( myResult.weaponItemId ) )
					break;	                                                                  
			}
			return myResult;
		}

		  
		                                                    
		  
		oSettings.loadoutSlot = GameInterfaceAPI.GetSettingString( 'ui_vanitysetting_loadoutslot_' + oSettings.team );
		oSettings.weaponItemId = LoadoutAPI.GetItemID( oSettings.team, oSettings.loadoutSlot );
		if ( !ItemInfo.IsWeapon( oSettings.weaponItemId ) )
		{	                                                                                              
			                                      
			var randomResult = _fnRollRandomLoadoutSlotAndWeapon( oSettings.team );
			oSettings.loadoutSlot = randomResult.loadoutSlot;
			oSettings.weaponItemId = randomResult.weaponItemId;
			                                                                                                
			                                                          
			GameInterfaceAPI.SetSettingString( 'ui_vanitysetting_loadoutslot_' + oSettings.team, oSettings.loadoutSlot );
		}

		  
		                  
		  
		oSettings.glovesItemId = LoadoutAPI.GetItemID( oSettings.team, 'clothing_hands' );

		  
		                                                                   
		  
		if ( !oSettings.charItemId )
			oSettings.charItemId = LoadoutAPI.GetItemID( oSettings.team, 'customplayer' );

		  
		                                                       
		                                                         
		                                        
		  
		if ( optionalState && optionalState === 'unowned' )
		{
			var randomResult = _fnRollRandomLoadoutSlotAndWeapon( oSettings.team );
			oSettings.loadoutSlot = randomResult.loadoutSlot;
			oSettings.weaponItemId = LoadoutAPI.GetDefaultItem( oSettings.team, oSettings.loadoutSlot );
			oSettings.glovesItemId = LoadoutAPI.GetDefaultItem( oSettings.team, 'clothing_hands' );
		}

		return oSettings;
	}

	var _GetStickerSlotCount = function( id )
	{
		return InventoryAPI.GetItemStickerSlotCount( id );
	};

	var _GetStickerCount = function( id )
	{
		return InventoryAPI.GetItemStickerCount( id );
	};

	var _GetitemStickerList = function( id )
	{
		var count = _GetStickerCount( id );
		var stickerList = [];

		for ( var i = 0; i < count; i++ )
		{
			var image = _GetStickerImageByIndex( id, i );
			var oStickerInfo = {
				image: _GetStickerImageByIndex( id, i ),
				name: _GetStickerNameByIndex( id, i )
			}
			stickerList.push( oStickerInfo );
		}

		return stickerList;
	};

	var _GetStickerImageByIndex = function( id, index )
	{
		return InventoryAPI.GetItemStickerImageByIndex( id, index );
	};

	var _GetStickerNameByIndex = function( id, index )
	{
		return InventoryAPI.GetItemStickerNameByIndex( id, index )
	};

	var _GetItemPickUpMethod = function( id )
	{
		return InventoryAPI.GetItemPickupMethod( id );
	};

	var _GetLoadoutPrice = function( id, subposition )
	{
		var team = _IsEquippedForCT( id ) ? 'ct' : 't';
		return LoadoutAPI.GetItemGamePrice( team, _GetSlotSubPosition( id ).toString() );
	};

	var _GetStoreOriginalPrice = function( id, count, rules )
	{
		                                                                      
		                                                                            
		                                                                                                   
		return StoreAPI.GetStoreItemOriginalPrice( id, count, rules ? rules : '' );
	};

	var _GetStoreSalePrice = function( id, count, rules )
	{
		                                                                      
		                                                                            
		                                                                                                   
		return StoreAPI.GetStoreItemSalePrice( id, count, rules ? rules : '' );
	};

	var _GetStoreSalePercentReduction = function( id, count )
	{
		return StoreAPI.GetStoreItemPercentReduction( id, count );                                                 
	};

	var _ItemPurchase = function( id )
	{
		                                                
		                                                    
		StoreAPI.StoreItemPurchase( id );
	};

	var _IsStatTrak = function( id )
	{
		var numIsStatTrak = InventoryAPI.GetRawDefinitionKey( id , "will_produce_stattrak" );

		return ( Number( numIsStatTrak ) === 1 ) ? true : false;
	};

	var _IsEquippalbleButNotAWeapon = function( id )
	{
		var subSlot = _GetSlotSubPosition( id );
		return ( subSlot === "flair0" || subSlot === "musickit" || subSlot === "spray0" || subSlot === "customplayer" );
	};

	var _IsEquippableThroughContextMenu = function( id )
	{
		var subSlot = _GetSlotSubPosition( id );
		return ( subSlot === "flair0" || subSlot === "musickit" || subSlot === "spray0" );
	};

	var _IsWeapon = function( id )
	{
		var schemaString = InventoryAPI.BuildItemSchemaDefJSON( id );

		if ( !schemaString )
			return false;
		
		var itemSchemaDef = JSON.parse( schemaString );	
		
		return ( itemSchemaDef[ "craft_class" ] === "weapon" );
	};

	var _IsCharacter = function( id )
	{
		return ( _GetSlotSubPosition( id ) === "customplayer" );
	}

	var _IsItemCt = function( id )
	{
		return _GetTeam( id ) === '#CSGO_Inventory_Team_CT';
	};
		
	var _IsItemT = function( id )
	{
		return _GetTeam( id ) === '#CSGO_Inventory_Team_T';
	};

	var _IsItemAnyTeam = function( id )
	{
		return _GetTeam( id ) === '#CSGO_Inventory_Team_Any';
	};

	var _GetItemDefinitionName = function( id )
	{
		return InventoryAPI.GetItemDefinitionName( id );
	};

	var _ItemMatchDefName = function( id, defName )
	{
		return InventoryAPI.DoesItemMatchDefinitionByName( id, defName );
	};

	var _ItemDefinitionNameSubstrMatch = function( id, defSubstr )
	{
		var itemDefName = InventoryAPI.GetItemDefinitionName( id );
		return ( itemDefName && ( itemDefName.indexOf( defSubstr ) != -1 ) );
	};

	var _GetFauxReplacementItemID = function( id, purpose )
	{
		                                                                                 
		                                                                                
		                                
		if ( purpose === 'graffiti' )
		{
			if ( _ItemDefinitionNameSubstrMatch( id, 'tournament_journal_' ) )
			{
				return _GetFauxItemIdForGraffiti( parseInt( InventoryAPI.GetItemAttributeValue( id, 'sticker slot 0 id' ) ));
			}
		}
		return id;
	};

	var _GetFauxItemIdForGraffiti = function( stickestickerid_graffiti )
	{
		                                                                                 
		                                                                                
		                                
		
		return InventoryAPI.GetFauxItemIDFromDefAndPaintIndex(                
			1349, stickestickerid_graffiti );
	};

	var _GetItemIdForItemEquippedInLoadoutSlot = function( id, team )
	{
		return LoadoutAPI.GetItemID( team, _GetSlotSubPosition( id ) );
	};

	                                              
	    
	   	                                                  
	   	
	   	                                                          
	   	                                                        
	   	 
	   		                                        
	   		 
	   			                                  
	   		    
	   	    
	     
	
	var _ItemsNeededToTradeUp = function( id )
	{
		return InventoryAPI.GetNumItemsNeededToTradeUp( id );
	};

	var _GetGifter = function( id )
	{
		var xuid = InventoryAPI.GetItemGifterXuid( id );
		
		return xuid !== undefined ? xuid : '';
	};

	var _GetSet = function( id )
	{
		var setName = InventoryAPI.GetSet( id );

		return setName !== undefined ? setName : '';
	};

	var _GetModelPath = function( id, itemSchemaDef )
	{
		var isMusicKit = _ItemMatchDefName( id, 'musickit' );
		var issMusicKitDefault = _ItemMatchDefName( id, 'musickit_default' );
		var isSticker = _ItemMatchDefName( id, 'sticker' );
		var isSpray = itemSchemaDef.name === 'spraypaint';
		var isSprayPaint = itemSchemaDef.name === 'spray';
		var isFanTokenOrShieldItem = itemSchemaDef.name && itemSchemaDef.name.indexOf( 'tournament_journal_' ) != -1;
	
		                                                                    
		                                                       
		if ( isSpray || isSprayPaint || isFanTokenOrShieldItem )
			return 'vmt://spraypreview_' + id;
		else if ( isSticker )
			return 'vmt://stickerpreview_' + id;
		else if ( itemSchemaDef.hasOwnProperty( "model_player" ) || isMusicKit || issMusicKitDefault )
			return 'img://inventory_' + id;
	};

	                                                                        
	var _GetModelPlayer = function( id )
	{
		var schemaString = InventoryAPI.BuildItemSchemaDefJSON( id );
		var itemSchemaDef = JSON.parse( schemaString );		

		var modelPlayer = itemSchemaDef[ "model_player" ];

		return modelPlayer;

	}

	var _GetDefaultCheer = function( id )
	{
		var schemaString = InventoryAPI.BuildItemSchemaDefJSON( id );
		var itemSchemaDef = JSON.parse( schemaString );		

		if ( itemSchemaDef[ "default_cheer" ] )
			return itemSchemaDef[ "default_cheer" ]
		else
			return "";
	}

	var _GetVoPrefix = function( id )
	{
		var schemaString = InventoryAPI.BuildItemSchemaDefJSON( id );
		var itemSchemaDef = JSON.parse( schemaString );		

		return itemSchemaDef[ "vo_prefix" ];
	}

	var _GetModelPathFromJSONOrAPI = function( id )
	{
		                                
		if ( id === '' || id === undefined || id === null )
		{
			return '';
		}
		
		var pedistalModel = '';
		var schemaString = InventoryAPI.BuildItemSchemaDefJSON( id );
		var itemSchemaDef = JSON.parse( schemaString );
		
		if ( _GetSlotSubPosition( id ) === "flair0" )
		{
			pedistalModel = itemSchemaDef.hasOwnProperty( 'attributes' ) ? itemSchemaDef.attributes[ "pedestal display model" ] : '';
		}
		else if( _ItemHasCapability( id, 'decodable' ) )
		{
			                                  

			pedistalModel = itemSchemaDef.hasOwnProperty( "model_player" ) ? itemSchemaDef.model_player : '';
			                                                   
		}

		return ( pedistalModel === '' ) ? _GetModelPath( id, itemSchemaDef ) : pedistalModel;
	};

	var _GetLootListCount = function( id )
	{
		return InventoryAPI.GetLootListItemsCount( id );
	};

	var _GetLootListItemByIndex = function( id, index )
	{
		return InventoryAPI.GetLootListItemIdByIndex( id, index );
	};

	var _GetMarketLinkForLootlistItem = function( id )
	{
		var appID = SteamOverlayAPI.GetAppID();
		var communityUrl = SteamOverlayAPI.GetSteamCommunityURL();
		var strName = _GetName( id );
		
		return communityUrl + "/market/search?appid=" + appID + "&lock_appid=" + appID + "&q=" + strName;
	};

	var _GetToolType = function( id )
	{
		return InventoryAPI.GetToolType( id );
	};

	function _IsDefaultCharacter ( id )
	{
		var defaultTItem = LoadoutAPI.GetDefaultItem( 't', 'customplayer' );
		var defaultCTItem = LoadoutAPI.GetDefaultItem( 'ct', 'customplayer' );
		return id == defaultTItem || id == defaultCTItem;
	}

	function _IsPreviewable ( id )
	{
		return ( ItemInfo.GetSlotSubPosition( id ) || ItemInfo.ItemMatchDefName( id, 'sticker' ) || ItemInfo.ItemMatchDefName( id, 'spray' ) ) &&
			!ItemInfo.ItemDefinitionNameSubstrMatch( id, 'tournament_journal_' ) &&                                                       
			!_IsDefaultCharacter( id );
	}

	return {
		GetRarityColor					: _GetRarityColor,
		GetName							: _GetName,
		GetFauxReplacementItemID		: _GetFauxReplacementItemID,
		GetFauxItemIdForGraffiti		: _GetFauxItemIdForGraffiti,
		GetFormattedName				: _GetFormattedName,                                 
		IsEquipped						: _IsEquipped,
		IsEquippedForCT					: _IsEquippedForCT,
		IsEquippedForT					: _IsEquippedForT,
		IsEquippedForNoTeam				: _IsEquippedForNoTeam,
		IsEquippalbleButNotAWeapon		: _IsEquippalbleButNotAWeapon,
		IsEquippableThroughContextMenu  : _IsEquippableThroughContextMenu,
		AddItemToShuffle				: _AddItemToShuffle,
		RemoveItemFromShuffle			: _RemoveItemFromShuffle,
		IsItemInShuffleForTeam			: _IsItemInShuffleForTeam,
		ClearShuffle					: _ClearShuffle,
		SetShuffleEnabled				: _SetShuffleEnabled, 
		IsShuffleEnabled				: _IsShuffleEnabled, 
		IsShuffleAllowed				: _IsShuffleAllowed, 
		CountItemsInInventoryForShuffleSlot	: _CountItemsInInventoryForShuffleSlot, 
		EnsureShuffleItemEquipped		: _EnsureShuffleItemEquipped, 
		GetSlot							: _GetSlot,
		GetTeam							: _GetTeam,
		GetSlotSubPosition				: _GetSlotSubPosition,
		IsSpraySealed					: _IsSpraySealed,
		IsSprayPaint					: _IsSprayPaint,
		IsTradeUpContract				: _IsTradeUpContract,
		GetSprayTintColor				: _GetSprayTintColor,
		IsTool							: _IsTool,
		IsWeapon						: _IsWeapon,
		IsCharacter						: _IsCharacter,
		GetCapabilitybyIndex			: _GetCapabilitybyIndex,
		GetCapabilityCount				: _GetCapabilityCount,
		ItemHasCapability				: _ItemHasCapability,
		GetChosenActionItemsCount		: _GetChosenActionItemsCount,
		GetChosenActionItemIDByIndex	: _GetChosenActionItemIDByIndex,
		GetStickerSlotCount				: _GetStickerSlotCount,
		GetStickerCount					: _GetStickerCount,
		GetitemStickerList				: _GetitemStickerList,
		GetItemPickUpMethod				: _GetItemPickUpMethod,
		BIsRewardPremium				: function( id ) { return InventoryAPI.BIsRewardPremium( id ) },
		GetRewardTier					: function( id ) { return InventoryAPI.GetRewardTier( id ) },
		GetLoadoutPrice					: _GetLoadoutPrice,
		GetStoreOriginalPrice			: _GetStoreOriginalPrice,
		GetStoreSalePrice				: _GetStoreSalePrice,
		GetStoreSalePercentReduction	: _GetStoreSalePercentReduction,
		ItemPurchase					: _ItemPurchase,
		IsItemCt						: _IsItemCt,
		IsItemT							: _IsItemT,
		IsItemAnyTeam					: _IsItemAnyTeam,
		ItemsNeededToTradeUp			: _ItemsNeededToTradeUp,
		             						                                      
		ItemMatchDefName				: _ItemMatchDefName,
		ItemDefinitionNameSubstrMatch	: _ItemDefinitionNameSubstrMatch,
		GetItemDefinitionName			: _GetItemDefinitionName,
		GetGifter						: _GetGifter,
		GetSet							: _GetSet,
		GetModelPath					: _GetModelPath,
		GetModelPathFromJSONOrAPI       : _GetModelPathFromJSONOrAPI,
		GetModelPlayer                  : _GetModelPlayer,
		GetLootListCount				: _GetLootListCount,
		GetLootListItemByIndex			: _GetLootListItemByIndex,
		IsStatTrak						: _IsStatTrak,
		GetToolType						: _GetToolType,
		GetMarketLinkForLootlistItem	: _GetMarketLinkForLootlistItem,
		GetItemIdForItemEquippedInLoadoutSlot: _GetItemIdForItemEquippedInLoadoutSlot,           
		IsLoadoutSlotSubPositionAWeapon	: _IsLoadoutSlotSubPositionAWeapon,
		GetDefaultCheer					: _GetDefaultCheer,
		GetVoPrefix						: _GetVoPrefix,
		IsPreviewable					: _IsPreviewable,
		IsDefaultCharacter				: _IsDefaultCharacter,
		GetKeyForCaseInXray				: _GetKeyForCaseInXray,
		GetItemsInXray					: _GetItemsInXray,
		GetOrUpdateVanityCharacterSettings: _GetOrUpdateVanityCharacterSettings,
		DeepCopyVanityCharacterSettings : _DeepCopyVanityCharacterSettings,
		PrecacheVanityCharacterSettings : _PrecacheVanityCharacterSettings,
		GetLoadoutWeapons				: _GetLoadoutWeapons
	};
})();

;                                                             

"use strict";

var LicenseUtil = ( function ()
{
	var _GetCurrentLicenseRestrictions = function()
	{
		var szButtonText = "#Store_Get_License";
		var szMessageText = "#SFUI_LoginLicenseAssist_NoOnlineLicense";
		switch ( MyPersonaAPI.GetLicenseType() )
		{
		case "free_pw_needlink":
			szButtonText = "#Store_Link_Accounts";
			szMessageText = "#SFUI_LoginLicenseAssist_PW_NeedToLinkAccounts";
			break;
		case "free_pw_needupgrade":
			szMessageText = "#SFUI_LoginLicenseAssist_HasLicense_PW";
			break;
		case "free_pw":
			szMessageText = "#SFUI_LoginLicenseAssist_NoOnlineLicense_PW";
			break;
		case "purchased":
			return false;
		}

		return {
			license_msg : szMessageText,
			license_act : szButtonText
		};
	}

	var _ShowLicenseRestrictions = function( restrictions )
	{
		if ( restrictions !== false )
		{
			                              
			return UiToolkitAPI.ShowGenericPopupYesNo(
				$.Localize( restrictions.license_act ),
				$.Localize( restrictions.license_msg ),
				'',
				function() { MyPersonaAPI.ActionBuyLicense(); },
				function() {}
			);
		}
	}

	return{
		GetCurrentLicenseRestrictions : _GetCurrentLicenseRestrictions,
		ShowLicenseRestrictions : _ShowLicenseRestrictions
	};
})();