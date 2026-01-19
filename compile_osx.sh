#!/bin/zsh

xcodebuild clean -project Esatto.xcodeproj && xcodebuild clean -project Arco.xcodeproj && xcodebuild -project Esatto.xcodeproj && xcodebuild -project Arco.xcodeproj

