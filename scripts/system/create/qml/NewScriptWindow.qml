//
//  NewScriptWindow.qml
//  qml/hifi
//
//  Created by HifiExperiments on 1/21/2925
//  Copyright 2025 Overte e.V.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

import QtQuick 2.7
import QtQuick.Controls 2.2

StackView {
    id: stackView
    anchors.fill: parent
    anchors.leftMargin: 10
    anchors.rightMargin: 10
    anchors.topMargin: 40

    signal sendToScript(var message);

    NewScriptDialog {
        id: dialog
        anchors.fill: parent
        Component.onCompleted:{
            dialog.sendToScript.connect(stackView.sendToScript);
        }
    }
}
