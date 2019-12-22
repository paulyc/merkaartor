//
// C++ Implementation: Global
//
// Description:
//
//
// Author: cbro <cbro@semperpax.com>, (C) 2010
//
// Copyright: See COPYING file that comes with this distribution
//
//

#include "Global.h"
#include "MainWindow.h"
#include "SlippyMapWidget.h"

#ifdef PORTABLE_BUILD
bool g_Merk_Portable = true;
#else
bool g_Merk_Portable = false;
#endif
bool g_Merk_Frisius = true;
bool g_Merk_NoGuardedTagsImport = false;
bool g_Merk_Segment_Mode = false;
bool g_Merk_Ignore_Preferences = false;
bool g_Merk_Reset_Preferences = false;
bool g_Merk_IgnoreStartupTemplate = false;
#if QT_VERSION < 0x040700 || defined(FORCE_46)
bool g_Merk_SelfClip = true;
#else
bool g_Merk_SelfClip = false;
#endif

MainWindow* g_Merk_MainWindow = nullptr;
MemoryBackend g_backend;
SlippyMapCache* SlippyMapWidget::theSlippyCache = 0;

QStringList tagKeys;
QHash<QString, quint32> tagKeysHash;
QStringList tagValues;
QHash<QString, quint32> tagValuesHash;
QHash< quint32, QList<quint32> > tagList;
QStringList userList;
QString noUser;

QPair<quint32, quint32> g_addToTagList(QString k, QString v)
{
    qint32 ik, iv;

    if (!tagKeysHash.contains(k)) {
        tagKeys.append(k);
        ik = tagKeys.size()-1;
        tagKeysHash[k] = ik;
    } else
        ik = tagKeysHash.value(k);

    if (!tagValuesHash.contains(v)) {
        tagValues.append(v);
        iv = tagValues.size()-1;
        tagValuesHash[v] = iv;
    } else
        iv = tagValuesHash.value(v);

    if (!k.isEmpty() && !v.isEmpty())
        tagList[ik].append(iv);

    return qMakePair((quint32)ik, (quint32)iv);
}

void g_removeFromTagList(quint32 k, quint32 v)
{
    tagList[k].removeOne(v);
    if (tagList[k].isEmpty())
        tagList.remove(k);
}

QStringList g_getTagKeys()
{
    return tagKeys;
}

QStringList g_getTagValues()
{
    return tagValues;
}

QStringList g_getTagValueList(QString k)
{
    QSet<quint32> retList;
    if (k == "*") {
        foreach (QList<quint32> list, tagList) {
            retList.unite(QSet<quint32>(list.begin(), list.end()));
        }
    } else {
        const QList<quint32> &list = tagList[tagKeys.indexOf(k)];
        retList = QSet(list.begin(), list.end());
    }

    QStringList res;
    foreach (quint32 i, retList)
        res << g_getTagValue(i);

    return res;
}

const QString& g_getTagKey(int idx)
{
    return tagKeys.at(idx);
}

quint32 g_getTagKeyIndex(const QString& s)
{
    return tagKeys.indexOf(s);
}

QStringList g_getTagKeyList()
{
    return QSet(tagKeys.begin(), tagKeys.end()).values();
}

QString g_getTagValue(int idx)
{
    return tagValues.at(idx);
}

quint32 g_getTagValueIndex(const QString& s)
{
    return tagValues.indexOf(s);
}

quint32 g_setUser(const QString& u)
{
    if (u.isEmpty())
        return 0xffffffff;

    qint32 ret = userList.indexOf(u);
    if (ret == -1) {
        userList.append(u);
        return userList.size()-1;
    }
    return ret;
}

const QString& g_getUser(quint32 idx)
{
    if (idx != 0xffffffff)
        return userList[idx];
    else
        return noUser;
}
