"use strict";
//
//  places.js
//
//  Created by Alezia Kurdis, January 1st, 2022.
//  Copyright 2022-2025 Overte e.V.
//
//  Generate an explore app based on the differents source of placename data.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//  SPDX-License-Identifier: Apache-2.0
//

(function() {
    var jsMainFileName = "places.js";
    var ROOT = Script.resolvePath('').split(jsMainFileName)[0];

    var metaverseServers = [];
    var SETTING_METAVERSE_TO_FETCH = "placesAppMetaverseToFetch";
    var SETTING_PINNED_METAVERSE = "placesAppPinnedMetaverse";
    var SETTING_MATURITY_FILTER = "placesAppMaturityFilter";
    var DEFAULT_MATURITY = ["adult", "mature", "teen", "everyone", "unrated"];
    var REQUEST_TIMEOUT = 10000; //10 seconds

    var httpRequest = null;
    var placesData;
    var portalList = [];

    var nbrPlacesNoProtocolMatch = 0;
    var nbrPlaceProtocolKnown = 0;

    var APP_NAME = "PLACES";
    var APP_URL = ROOT + "places.html";
    var APP_ICON_INACTIVE = ROOT + "icons/appicon_i.png";
    var APP_ICON_ACTIVE = ROOT + "icons/appicon_a.png";
    var appStatus = false;
    var channel = "com.overte.places";
    
    var portalChannelName = "com.overte.places.portalRezzer";
    var MAX_DISTANCE_TO_CONSIDER_PORTAL = 100.0; //in meters
    var PORTAL_DURATION_MILLISEC = 45000; //45 sec
    var rezzerPortalCount = 0;
    var MAX_REZZED_PORTAL = 15;
    
    var placesOfTheCurrentDomain = [];

    var tablet = Tablet.getTablet("com.highfidelity.interface.tablet.system");

    tablet.screenChanged.connect(onScreenChanged);

    var button = tablet.addButton({
        text: APP_NAME,
        icon: APP_ICON_INACTIVE,
        activeIcon: APP_ICON_ACTIVE,
        sortOrder: 8
    });
    
    var timestamp = 0;
    var INTERCALL_DELAY = 200; //0.3 sec
    var PERSISTENCE_ORDERING_CYCLE = 5 * 24 * 3600 * 1000; //5 days
    
    function clicked(){
        if (appStatus === true) {
            tablet.webEventReceived.disconnect(onAppWebEventReceived);
            tablet.gotoHomeScreen();
            appStatus = false;
        } else {
            tablet.gotoWebScreen(APP_URL);
            tablet.webEventReceived.connect(onAppWebEventReceived);
            appStatus = true;
        }

        button.editProperties({
            isActive: appStatus
        });
    }

    button.clicked.connect(clicked);


    function onAppWebEventReceived(message) {
        var d = new Date();
        var n = d.getTime();
        
        var messageObj = JSON.parse(message);
        if (messageObj.channel === channel) {
            if (messageObj.action === "READY_FOR_CONTENT" && (n - timestamp) > INTERCALL_DELAY) {
                d = new Date();
                timestamp = d.getTime();
                sendPersistedMaturityFilter();
                transmitPortalList();
                sendCurrentLocationToUI();

            } else if (messageObj.action === "TELEPORT" && (n - timestamp) > INTERCALL_DELAY) {
                d = new Date();
                timestamp = d.getTime();

                if (messageObj.address.length > 0) {
                    Window.location = messageObj.address;
                }
                
            } else if (messageObj.action === "REQUEST_PORTAL" && (n - timestamp) > INTERCALL_DELAY) {
                d = new Date();
                timestamp = d.getTime();
                var portalPosition = Vec3.sum(MyAvatar.feetPosition, Vec3.multiplyQbyV(MyAvatar.orientation, {"x": 0.0, "y": 0.0, "z": -2.0}));
                var requestToSend = {
                    "action": "REZ_PORTAL",
                    "position": portalPosition,
                    "url": messageObj.address,
                    "name": messageObj.name,
                    "placeID": messageObj.placeID
                };
                Messages.sendMessage(portalChannelName, JSON.stringify(requestToSend), false);
                
            } else if (messageObj.action === "COPY_URL" && (n - timestamp) > INTERCALL_DELAY) {
                d = new Date();
                timestamp = d.getTime();
                Window.copyToClipboard(messageObj.address);
                Window.displayAnnouncement("Place URL copied.");
            } else if (messageObj.action === "GO_HOME" && (n - timestamp) > INTERCALL_DELAY) {
                d = new Date();
                timestamp = d.getTime();
                if (LocationBookmarks.getHomeLocationAddress()) {
                    location.handleLookupString(LocationBookmarks.getHomeLocationAddress());
                } else {
                    Window.location = "file:///~/serverless/tutorial.json";
                }                
            } else if (messageObj.action === "GO_BACK" && (n - timestamp) > INTERCALL_DELAY) {
                d = new Date();
                timestamp = d.getTime();
                location.goBack();
            } else if (messageObj.action === "GO_FORWARD" && (n - timestamp) > INTERCALL_DELAY) {
                d = new Date();
                timestamp = d.getTime();
                location.goForward();
            } else if (messageObj.action === "PIN_META" && (n - timestamp) > INTERCALL_DELAY) {
                d = new Date();
                timestamp = d.getTime();
                metaverseServers[messageObj.metaverseIndex].pinned = messageObj.value;
                savePinnedMetaverseSetting();
            } else if (messageObj.action === "FETCH_META" && (n - timestamp) > INTERCALL_DELAY) {
                d = new Date();
                timestamp = d.getTime();
                metaverseServers[messageObj.metaverseIndex].fetch = messageObj.value;
                saveMetaverseToFetchSetting();
            } else if (messageObj.action === "ADD_MS" && (n - timestamp) > INTERCALL_DELAY) {
                d = new Date();
                timestamp = d.getTime();
                var newMs = {
                    "url": messageObj.metaverseUrl,
                    "region": "external",
                    "fetch": false,
                    "pinned": true,
                    "order": "Z"
                }
                metaverseServers.push(newMs);
                savePinnedMetaverseSetting();
            } else if (messageObj.action === "SET_MATURITY_FILTER" && (n - timestamp) > INTERCALL_DELAY) {
                d = new Date();
                timestamp = d.getTime();
                Settings.setValue(SETTING_MATURITY_FILTER, messageObj.filter);
            }
        }
    }

    function savePinnedMetaverseSetting() {
        var pinnedServers = [];
        for (var q = 0; q < metaverseServers.length; q++) {
            if (metaverseServers[q].pinned) {
                pinnedServers.push(metaverseServers[q].url);
            }
        }
        Settings.setValue(SETTING_PINNED_METAVERSE, pinnedServers);
    }

    function saveMetaverseToFetchSetting() {
        var fetchedServers = [];
        for (var q = 0; q < metaverseServers.length; q++) {
            if (metaverseServers[q].fetch) {
                fetchedServers.push(metaverseServers[q].url);
            }
        }
        Settings.setValue(SETTING_METAVERSE_TO_FETCH, fetchedServers);
    }

    function onHostChanged(host) {
        sendPersistedMaturityFilter();
        transmitPortalList();
        sendCurrentLocationToUI();
    }

    location.hostChanged.connect(onHostChanged);
    
    function sendCurrentLocationToUI() {
        var currentLocationMessage = {
            "channel": channel,
            "action": "CURRENT_LOCATION",
            "data": location.href
        };

        tablet.emitScriptEvent(currentLocationMessage);        
    }
    
    function onScreenChanged(type, url) {
        if (type == "Web" && url.indexOf(APP_URL) != -1) {
            appStatus = true;
        } else {
            appStatus = false;
        }
        
        button.editProperties({
            isActive: appStatus
        });
    }

    function transmitPortalList() {
        metaverseServers = [];
        buildMetaverseServerList();
        
        portalList = [];
        nbrPlacesNoProtocolMatch = 0;
        nbrPlaceProtocolKnown = 0;
        var extractedData;
        
        for (var i = 0; i < metaverseServers.length; i++ ) {
            if (metaverseServers[i].fetch === true) {
                extractedData = getContent(metaverseServers[i].url + "/api/v1/places?status=online&per_page=1000");
                if (extractedData === "") {
                    metaverseServers[i].error = true;
                } else {
                    metaverseServers[i].error = false;
                }
                try {
                    placesData = JSON.parse(extractedData);
                    processData(metaverseServers[i]);
                } catch(e) {
                    placesData = {};
                }
                httpRequest = null;
            }
        }

        addUtilityPortals();
        
        portalList.sort(sortOrder);
        
        var percentProtocolRejected = Math.floor((nbrPlacesNoProtocolMatch/nbrPlaceProtocolKnown) * 100);
        
        var warning = "";
        if (percentProtocolRejected > 50) {
            warning = "WARNING: " + percentProtocolRejected + "% of the places are not listed because they are running under a different protocol. Maybe consider to upgrade.";
        }

        var message = {
            "channel": channel,
            "action": "PLACE_DATA",
            "data": portalList,
            "warning": warning,
            "metaverseServers": metaverseServers
        };

        tablet.emitScriptEvent(message);
        
    };

    function sendPersistedMaturityFilter() {
        var messageSent = {
            "channel": channel,
            "action": "MATURITY_FILTER",
            "filter": Settings.getValue(SETTING_MATURITY_FILTER, DEFAULT_MATURITY)
        };
        tablet.emitScriptEvent(messageSent);
    }

    function getFederationData() {
        /*
        //If federation.json is got from the Metaverse Server (not implemented yet)
        var fedDirectoryUrl = AccountServices.metaverseServerURL + "/federation.json";
        var extractedFedData = getContent(fedDirectoryUrl);
        */

        /*
        //If federation.json is got from a web storage
        var fedDirectoryUrl = ROOT + "federation.json"; + "?version=" + Math.floor(Math.random() * 999999);
        var extractedFedData = getContent(fedDirectoryUrl);
        */

        //if federation.json is local, on the user installation
        var extractedFedData = JSON.stringify(Script.require("./federation.json"));

        return extractedFedData;

    }

    function buildMetaverseServerList () {
        var extractedFedData = getFederationData();

        var pinnedMetaverses = Settings.getValue(SETTING_PINNED_METAVERSE, []);
        var metaversesToFetch = Settings.getValue(SETTING_METAVERSE_TO_FETCH, []);

        var federation = [];
        try {
            federation = JSON.parse(extractedFedData);
        } catch(e) {
            federation = [];
        }        
        var currentFound = false;
        var region, pinned, fetch, order, metaverse;
        for (var i=0; i < federation.length; i++) {
            if (federation[i].node === AccountServices.metaverseServerURL) {
                region = "local";
                order = "A";
                fetch = true;
                pinned = false;
                currentFound = true;
            } else {
                region = "federation";
                order = "F";
                fetch = false;
                pinned = false;
            }
            
            metaverse = {
                "url": federation[i].node,
                "region": region,
                "fetch": fetch,
                "pinned": pinned,
                "order": order
            };
            metaverseServers.push(metaverse);
        }
        if (!currentFound) {
            metaverse = {
                "url": AccountServices.metaverseServerURL,
                "region": "local",
                "fetch": true,
                "pinned": false,
                "order": "A"
            };
            metaverseServers.push(metaverse);
        }
 
        for (i = 0; i < pinnedMetaverses.length; i++) {
            var target = pinnedMetaverses[i];
            var found = false;
            for (var k = 0; k < metaverseServers.length; k++) {
                if (metaverseServers[k].url === target) {
                    metaverseServers[k].pinned = true;
                    found = true;
                    break;
                }
            }
            if (!found) {
                metaverse = {
                    "url": target,
                    "region": "external",
                    "fetch": false,
                    "pinned": true,
                    "order": "Z"
                };
                metaverseServers.push(metaverse);
            }
        }

        for (i = 0; i < metaversesToFetch.length; i++) {
            var target = metaversesToFetch[i];
            for (var k = 0; k < metaverseServers.length; k++) {
                if (metaverseServers[k].url === target) {
                    metaverseServers[k].fetch = true;
                    break;
                }
            }
        }
        
        metaverseServers.sort(sortOrder);
    }

    function getContent(url) {
        httpRequest = new XMLHttpRequest();
        httpRequest.open("GET", url, false); // false for synchronous request
        httpRequest.setRequestHeader("Cache-Control", "no-cache");
        httpRequest.timeout = REQUEST_TIMEOUT;
        httpRequest.ontimeout=function(){ 
            return ""; 
        };        
        httpRequest.send( null );
        return httpRequest.responseText;
    }

    function processData(metaverseInfo){
        placesOfTheCurrentDomain = [];
        var supportedProtocole = Window.protocolSignature();
        
        var places = placesData.data.places;
        var i, j;
        for (i = 0;i < places.length; i++) {

            var region, category, accessStatus;
            
            var description = (places[i].description ? places[i].description : "");
            var thumbnail = (places[i].thumbnail ? places[i].thumbnail : "");
            if ( places[i].domain.protocol_version === supportedProtocole ) {
                  
                    region = metaverseInfo.order;

                    if ( thumbnail.substr(0, 4).toLocaleLowerCase() !== "http") {
                        category = "O"; //Other
                    } else {
                        category = "A"; //Attraction
                    }
                    
                    if (places[i].domain.num_users > 0) {
                        if (places[i].domain.num_users >= places[i].domain.capacity && places[i].domain.capacity !== 0) {
                            accessStatus = "FULL";
                        } else {
                            accessStatus = "LIFE";
                        }
                    } else {
                        accessStatus = "NOBODY";
                    }                 

                    var portal = {
                        "order": category + "_" + region + "_" + getSeededRandomForString(places[i].id),
                        "category": category,
                        "accessStatus": accessStatus,
                        "domainAccessStatus": accessStatus,
                        "name": places[i].name,
                        "description": description,
                        "thumbnail": thumbnail,
                        "maturity": places[i].maturity,
                        "address": places[i].address,
                        "current_attendance": places[i].domain.num_users,
                        "place_attendance": -1,
                        "id": places[i].id,
                        "visibility": places[i].visibility,
                        "capacity": places[i].domain.capacity,
                        "tags": getListFromArray(places[i].tags),
                        "managers": getListFromArray(places[i].managers),
                        "domain": places[i].domain.name,
                        "domainOrder": aplphabetize(zeroPad(places[i].domain.num_users, 6)) + "_" + places[i].domain.name + "_" + places[i].name,
                        "metaverseServer": metaverseInfo.url,
                        "metaverseRegion": metaverseInfo.region
                    };
                    portalList.push(portal);
                    
                    if ("{" + places[i].domain.id + "}" === location.domainID) {
                        placesOfTheCurrentDomain.push({"index": (portalList.length - 1), "position": getPositionFromPath(places[i].path)});
                    }

            } else {
                nbrPlacesNoProtocolMatch++;
            }
        }
        
        //counting for current domain's places count
        if (placesOfTheCurrentDomain.length > 0) {
            var presences = AvatarList.getAvatarIdentifiers();
            var avatarsPlace = [];
            for (i = 0;i < presences.length; i++) {
                var avatarPosition = AvatarList.getAvatar(presences[i]).position;
                var minDistance = 100000;
                var leadingIndex = -1;
                for (j = 0; j < placesOfTheCurrentDomain.length; j++) {
                    var avatarPlacedistance = Vec3.distance(placesOfTheCurrentDomain[j].position, avatarPosition);
                    if (avatarPlacedistance <= minDistance) {
                        leadingIndex = placesOfTheCurrentDomain[j].index;
                        minDistance = avatarPlacedistance;
                    }
                }
                avatarsPlace.push(leadingIndex);
            }
            for (i = 0;i < placesOfTheCurrentDomain.length; i++) {
                var count = 0;
                for (j = 0;j < avatarsPlace.length; j++) {
                    if (avatarsPlace[j] === placesOfTheCurrentDomain[i].index) {
                        count = count + 1;
                    }
                }
                portalList[placesOfTheCurrentDomain[i].index].place_attendance = count;
                portalList[placesOfTheCurrentDomain[i].index].current_attendance = presences.length;
                if (count === 0) {
                    portalList[placesOfTheCurrentDomain[i].index].accessStatus = "NOBODY";
                }
            }
        }
        
        nbrPlaceProtocolKnown = nbrPlaceProtocolKnown + places.length;
    
    }

    function getPositionFromPath(path) {
        var position = {"x": 0.0, "y": 0.0, "z": 0.0};
        var extracted = path.split("/");
        if (extracted.length > 1) {
            var stringXyz = extracted[1].split(",");
            position.x = parseFloat(stringXyz[0]);
            position.y = parseFloat(stringXyz[1]);
            position.z = parseFloat(stringXyz[2]);
        }
        return position;
    }

    function addUtilityPortals() {
        var localHostPortal = {
            "order": "Z_Z_AAAAAA",
            "category": "Z",
            "accessStatus": "NOBODY",
            "name": "localhost",
            "description": "",
            "thumbnail": "",
            "maturity": "unrated",
            "address": "localhost",
            "current_attendance": 0,
            "id": "",
            "visibility": "open",
            "capacity": 0,
            "tags": "",
            "managers": "",
            "domain": "",
            "domainOrder": "ZZZZZZZZZZZZZZA",
            "metaverseServer": "",
            "metaverseRegion": "local"
        };
        portalList.push(localHostPortal);

        var tutorialPortal = {
            "order": "Z_Z_AAAAAZ",
            "category": "Z",
            "accessStatus": "NOBODY",
            "name": "tutorial",
            "description": "",
            "thumbnail": "",
            "maturity": "unrated",
            "address": "file:///~/serverless/tutorial.json",
            "current_attendance": 0,
            "id": "",
            "visibility": "open",
            "capacity": 0,
            "tags": "",
            "managers": "",
            "domain": "",
            "domainOrder": "ZZZZZZZZZZZZZZZ",
            "metaverseServer": "",
            "metaverseRegion": "local"
        };
        portalList.push(tutorialPortal);
        
    }

    function aplphabetize(num) {
        var numbstring = num.toString();
        var newChar = "JIHGFEDCBA";
        var refChar = "0123456789";
        var processed = "";
        for (var j=0; j < numbstring.length; j++) {
            processed = processed + newChar.substr(refChar.indexOf(numbstring.charAt(j)),1);
        }
        return processed;
    }

    function getListFromArray(dataArray) {
        var dataList = "";
        if (dataArray !== undefined && dataArray.length > 0) {
            for (var k = 0; k < dataArray.length; k++) {
                if (k !== 0) {
                    dataList += ", "; 
                }
                dataList += dataArray[k];
            }
            if (dataArray.length > 1){
                dataList += ".";
            }
        }
        
        return dataList;
    }

    function sortOrder(a, b) {
        var orderA = a.order.toUpperCase();
        var orderB = b.order.toUpperCase();
        if (orderA > orderB) {
            return 1;    
        } else if (orderA < orderB) {
            return -1;
        }
        if (a.order > b.order) {
            return 1;    
        } else if (a.order < b.order) {
            return -1;
        }
        return 0;
    }

    function zeroPad(num, places) {
        var zero = places - num.toString().length + 1;
        return Array(+(zero > 0 && zero)).join("0") + num;
    }

    function getFrequentPlaces(list) {
        var count = {};
        list.forEach(function(list) {
            count[list] = (count[list] || 0) + 1;
        });
        return count;
    }

    //####### seed random library ################
    var seed = 75;

    var seededRandom = function(max, min) {
        max = max || 1;
        min = min || 0;
        seed = (seed * 9301 + 49297) % 233280;
        var rnd = seed / 233280;
        return min + rnd * (max - min);
    }

    function getStringScore(str) {
        var score = 0;
        for (var j = 0; j < str.length; j++){
            score += str.charAt(j).charCodeAt(0) + 1;
        }
        return score;
    }

    function getSeededRandomForString(str) {
        var score = getStringScore(str);
        var d = new Date();
        var n = d.getTime();
        var currentSeed = Math.floor(n / PERSISTENCE_ORDERING_CYCLE);
        seed = score * currentSeed;
        return zeroPad(Math.floor(seededRandom() * 100000),5);
    }
    //####### END of seed random library ################

    function onMessageReceived(paramChannel, paramMessage, paramSender, paramLocalOnly) {
        if (paramChannel === portalChannelName) {
            var instruction = JSON.parse(paramMessage);
            if (instruction.action === "REZ_PORTAL") {
                generatePortal(instruction.position, instruction.url, instruction.name, instruction.placeID);
            }
        }
    }

    function generatePortal(position, url, name, placeID) {
        if (rezzerPortalCount <= MAX_REZZED_PORTAL) {
            var TOLERANCE_FACTOR = 1.1;
            if (Vec3.distance(MyAvatar.position, position) < MAX_DISTANCE_TO_CONSIDER_PORTAL) {
                var height = MyAvatar.userHeight * MyAvatar.scale * TOLERANCE_FACTOR;
                
                var portalPosition = Vec3.sum(position, {"x": 0.0, "y": height/2, "z": 0.0});
                var dimensions = {"x": height * 0.618, "y": height, "z": height * 0.618};
                var userdata = {
                    "url": url,
                    "name": name,
                    "placeID": placeID
                };
                
                var portalID = Entities.addEntity({
                    "position": portalPosition,
                    "dimensions": dimensions,
                    "type": "Shape",
                    "shape": "Sphere",
                    "name": "Portal to " + name,
                    "canCastShadow": false,
                    "collisionless": true,
                    "userData": JSON.stringify(userdata),
                    "script": ROOT + "portal.js",
                    "visible": "false",
                    "grab": {
                        "grabbable": false
                    }
                }, "local");
                rezzerPortalCount = rezzerPortalCount + 1;
                
                Script.setTimeout(function () {
                    Entities.deleteEntity(portalID);
                    rezzerPortalCount = rezzerPortalCount - 1;
                    if (rezzerPortalCount < 0) {
                        rezzerPortalCount = 0;
                    }
                }, PORTAL_DURATION_MILLISEC);
            }
        }
    }

    function cleanup() {

        if (appStatus) {
            tablet.gotoHomeScreen();
            tablet.webEventReceived.disconnect(onAppWebEventReceived);
        }

        Messages.messageReceived.disconnect(onMessageReceived);
        Messages.unsubscribe(portalChannelName);

        tablet.screenChanged.disconnect(onScreenChanged);
        tablet.removeButton(button);
    }

    Messages.subscribe(portalChannelName);
    Messages.messageReceived.connect(onMessageReceived);

    Script.scriptEnding.connect(cleanup);
}());
