//
// C++ Interface: TagTemplate
//
// Description:
//
//
// Author: Chris Browet <cbro@semperpax.com>, (C) 2008
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef TAGTEMPLATE_H
#define TAGTEMPLATE_H

#include <QWidget>
#include <QList>
#include <QHash>
#include <QDomElement>
#include <QUrl>
#include <QVariant>

#include "TagSelector.h"

class MapView;
class Feature;
class QGroupBox;
class QDomElement;
class QComboBox;
class TagTemplates;
class CommandList;
class SetTagCommand;
class TagTemplateWidgetValue;

class TagTemplateWidget: public QObject
{
    Q_OBJECT

    public:
        TagTemplateWidget();
        virtual ~TagTemplateWidget();

    public:
        QString id() {return (!theId.isEmpty() ? theId : theTag);}
        QString tag() {return theTag;}
        virtual QWidget*	getWidget(const Feature* /* F */, const MapView* /*V*/) {return nullptr;}
        virtual TagTemplateWidgetValue* getCurrentValue() {return nullptr;}

        virtual void apply(const Feature*) {}

        virtual bool toXML(QDomElement& /* xParent */, bool /* header */) {return false;}
        static TagTemplateWidget*		fromXml(const QDomElement& e);

        void parseCommonElements(const QDomElement& e);
        void generateCommonElements(QDomElement& e);

    protected:
        QString		theId;
        QString		theTag;
        QString		theType;
        QWidget*	theMainWidget;
        QWidget*	theWidget;
        QWidget*	theLabelWidget;
        QString		theDescription;
        TagSelector*	theSelector;
        QList<TagTemplateWidgetValue*>	theValues;

    public:
        QHash<QString, QString>	theDescriptions;
        QUrl		theUrl;

    signals:
        void tagCleared(QString k);
        void tagChanged(QString k, QString v);
};

class TagTemplateWidgetValue: public TagTemplateWidget
{
    Q_OBJECT

    public:
        TagTemplateWidgetValue(const QString& aTagValue) : theTagValue(aTagValue) {}

    public:
        QString		theTagValue;

    public:
        virtual bool toXML(QDomElement& xParent, bool /* header */);
        static TagTemplateWidgetValue*		fromXml(const QDomElement& e);

};

Q_DECLARE_METATYPE( TagTemplateWidgetValue * );

class TagTemplateWidgetCombo: public TagTemplateWidget
{
    Q_OBJECT

    public:
        virtual ~TagTemplateWidgetCombo();

    public:
        QWidget*	getWidget(const Feature* F, const MapView* V);

        virtual void apply(const Feature* F);

        static TagTemplateWidgetCombo*		fromXml(const QDomElement& e);
        bool toXML(QDomElement& xParent, bool header);

    public slots:
        void on_combo_activated(int idx);
};

class TagTemplateWidgetYesno: public TagTemplateWidget
{
    Q_OBJECT

    public:
        QWidget*	getWidget(const Feature* F, const MapView* V);

        virtual void apply(const Feature* F);

        static TagTemplateWidgetYesno*		fromXml(const QDomElement& e);
        bool toXML(QDomElement& xParent, bool header);

    public slots:
        void on_checkbox_stateChanged(int state);
};

class TagTemplateWidgetConstant: public TagTemplateWidget
{
    Q_OBJECT

    public:
        ~TagTemplateWidgetConstant();

    public:
        QWidget*	getWidget(const Feature* F, const MapView* V);

        virtual void apply(const Feature* F);

        static TagTemplateWidgetConstant*		fromXml(const QDomElement& e);
        bool toXML(QDomElement& xParent, bool header);
};

class TagTemplateWidgetEdit: public TagTemplateWidget
{
    Q_OBJECT

    public:
        QWidget*	getWidget(const Feature* F, const MapView* V);

        virtual void apply(const Feature* F);

        static TagTemplateWidgetEdit*		fromXml(const QDomElement& e);
        bool toXML(QDomElement& xParent, bool header);

    public slots:
        void on_editingFinished();
};

class TagTemplate: public QObject
{
    Q_OBJECT

    friend class TagTemplates;

    public:
        TagTemplate();
        TagTemplate(QString aName);
        TagTemplate(QString aName, QString aSelector);
        TagTemplate(const TagTemplate& aTemplate);

        ~TagTemplate();

    public:
        TagSelectorMatchResult		matchesTag(const Feature* F, const MapView* V);
        QWidget*	getWidget(const Feature* F, const MapView* V);

        void setSelector(const QString& aName);
        void setSelector(TagSelector* aSelector);

        virtual void apply(const Feature* F);

        bool toXML(QDomElement& xParent);
        static TagTemplate*		fromXml(const QDomElement& e, TagTemplates* templates);

    protected:
        QWidget*		theWidget;
        TagSelector*	theSelector;
        QList < TagTemplateWidget* >	theFields;
        QHash<QString, QString>	theDescriptions;

    public slots:
        void on_tag_cleared(QString k);
        void on_tag_changed(QString k, QString vaL);

    signals:
        void tagCleared(QString k);
        void tagChanged(QString k, QString v);
};

Q_DECLARE_METATYPE( TagTemplate * );

class TagTemplates : public QObject
{
    Q_OBJECT

    public:
        TagTemplates();
        ~TagTemplates();

    public:
        QWidget*	getWidget(const Feature* F, const MapView* V);
        QList<TagTemplate*> items;
        TagTemplate* match(const Feature* F, const MapView* V);

        void addWidget(TagTemplateWidget* aWidget);
        TagTemplateWidget* findWidget(const QString& tag);

        virtual void apply(const Feature* F);

        static TagTemplates*	fromXml(const QDomElement& e);
        bool mergeXml(const QDomElement& e);
        bool toXML(QDomDocument& doc);

    protected:
        QList<TagTemplateWidget*> widgets;
        QWidget*		theWidget;
        QComboBox*		theCombo;
        const Feature*		theFeature;
        TagTemplate*	curTemplate;
        TagTemplate*	forcedTemplate;

    public slots:
        void on_combo_activated(int idx);
        void on_tag_cleared(QString k);
        void on_tag_changed(QString k, QString v);

    signals:
        void tagCleared(QString k);
        void tagChanged(QString k, QString v);
        void templateChanged(TagTemplate* aNewTemplate);
};


#endif // TAGTEMPLATE_H
