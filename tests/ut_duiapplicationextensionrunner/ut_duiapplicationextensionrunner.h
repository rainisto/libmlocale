/***************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (directui@nokia.com)
**
** This file is part of libdui.
**
** If you have questions regarding the use of this file, please contact
** Nokia at directui@nokia.com.
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation
** and appearing in the file LICENSE.LGPL included in the packaging
** of this file.
**
****************************************************************************/

#ifndef _UT_DUIAPPLICATIONEXTENSIONRUNNER_
#define _UT_DUIAPPLICATIONEXTENSIONRUNNER_

#include <QObject>
#include <QHash>
#include <duiapplicationextensioninterface.h>

class DuiApplicationExtensionRunner;
class DuiApplicationExtensionMetaData;
class DuiWidget;

class TestExtension : public QObject, public DuiApplicationExtensionInterface
{
public:
    virtual bool initialize(const QString &interface);
    virtual DuiWidget *widget();

    DuiWidget *widget_;
};

// Test case must inherit QObject
class Ut_DuiApplicationExtensionRunner: public QObject
{
    Q_OBJECT

private:
    /** DuiApplicationExtensionRunner instance under testing. */
    DuiApplicationExtensionRunner *m_instance;
    DuiApplicationExtensionMetaData *meta;
    TestExtension *extension;

public:
    static bool timerImmediateTimeout;
    static bool quitCalled;
    static QHash<QString, QString> gConfKeyMap;

private slots:
    void init();
    void cleanup();
    void initTestCase();
    void cleanupTestCase();

    void testInitializationSuccess();
    void testInitializationFailInvalidMetadata();
    void testInitializationFailConstructWidgetFails();
};

#endif //_UT_DUIAPPLICATIONEXTENSIONRUNNER_
