INCLUDEPATH += $$MERKAARTOR_SRC_DIR/Utils
DEPENDPATH += $$MERKAARTOR_SRC_DIR/Utils

HEADERS += \
    LineF.h \
    ShortcutOverrideFilter.h \
    SlippyMapWidget.h \
    EditCompleterDelegate.h \
    PictureViewerDialog.h \
    PixmapWidget.h \
    RemoteControlServer.hpp \
    SelectionDialog.h \
    SvgCache.h \
    MDiscardableDialog.h \
    OsmLink.h \
    Utils.h \
    TagSelector.h \
    TagSelectorWidget.h \
    CheckBoxList.h

SOURCES += \
    ShortcutOverrideFilter.cpp \
    SlippyMapWidget.cpp \
    EditCompleterDelegate.cpp \
    PictureViewerDialog.cpp \
    PixmapWidget.cpp \
    RemoteControlServer.cpp \
    SelectionDialog.cpp \
    SvgCache.cpp \
    MDiscardableDialog.cpp \
    OsmLink.cpp \
    Utils.cpp \
    TagSelector.cpp \
    TagSelectorWidget.cpp \
    CheckBoxList.cpp

FORMS += \
    PictureViewerDialog.ui \
    SelectionDialog.ui \
    TagSelectorWidget.ui \

RESOURCES += \
    Utils.qrc

isEmpty(MOBILE) {
  HEADERS += \
    ProjectionChooser.h

  SOURCES += \
    ProjectionChooser.cpp

  FORMS += \
    ProjectionChooser.ui
}
