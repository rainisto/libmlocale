/***************************************************************************
**
** Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (directui@nokia.com)
**
** This file is part of libmeegotouch.
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

#include <QPixmap>
#include <mwidgetstyle.h>
#include "mcontentitem.h"
#include "mcontentitem_p.h"
#include "mcontentitemmodel.h"
#include <QDebug>

#include "mwidgetcreator.h"
#include <mprogressindicator.h>
#include <mlabel.h>
M_REGISTER_WIDGET(MContentItem)

MContentItemPrivate::MContentItemPrivate():
    MWidgetControllerPrivate(),
    smallItem(0)
{
}

MContentItemPrivate::~MContentItemPrivate()
{
}

MContentItem::MContentItem(MContentItem::ContentItemStyle itemStyle, QGraphicsItem *parent)
    : MWidgetController(new MContentItemPrivate, new MContentItemModel, parent)
{
    model()->setItemStyle(itemStyle);
}

MContentItem::MContentItem(MContentItemPrivate *dd, MContentItemModel *model, QGraphicsItem *parent)
    : MWidgetController(dd, model, parent)
{
}

MContentItem::~MContentItem()
{

}

QPixmap MContentItem::pixmap() const
{
    Q_D(const MContentItem);
    return d->pixmap;
}

QImage MContentItem::image() const
{
    Q_D(const MContentItem);
    return d->image;
}

QString MContentItem::title() const
{
    return model()->title();
}

QString MContentItem::subtitle() const
{
    return model()->subtitle();
}

MContentItem::ContentItemStyle MContentItem::itemStyle() const
{
    return (MContentItem::ContentItemStyle) model()->itemStyle();
}

MContentItem::ContentItemMode MContentItem::itemMode() const
{
    return (MContentItem::ContentItemMode) model()->itemMode();
}

void MContentItem::setPixmap(const QPixmap &pixmap)
{
    Q_D(MContentItem);
    d->pixmap = pixmap;
    model()->setItemPixmap(d->pixmap);
}

void MContentItem::setImage(const QImage &image)
{
    Q_D(MContentItem);
    d->image = image;
    model()->setItemImage(d->image);
}

void MContentItem::setTitle(const QString &text)
{
    model()->setTitle(text);
}

void MContentItem::setSubtitle(const QString &text)
{
    model()->setSubtitle(text);
}

void MContentItem::setItemMode(ContentItemMode mode)
{
    model()->setItemMode(mode);
}

void MContentItem::click()
{
    emit clicked();
}

QVariant MContentItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemSelectedHasChanged) {
        model()->setSelected(value.toBool());
    }

    return QGraphicsItem::itemChange(change, value);
}

void MContentItem::setOptionalPixmap(const QPixmap &pixmap)
{
    Q_D(MContentItem);
    d->optionalPixmap = pixmap;
    model()->setOptionalPixmap(d->optionalPixmap);
}

void MContentItem::setOptionalImage(const QImage &image)
{
    Q_D(MContentItem);
    d->optionalImage = image;
    model()->setOptionalImage(d->optionalImage);
}

QPixmap MContentItem::optionalPixmap() const
{
    Q_D(const MContentItem);
    return d->optionalPixmap;
}

QImage MContentItem::optionalImage() const
{
    Q_D(const MContentItem);
    return d->optionalImage;
}

void MContentItem::setAdditionalItem(MWidget* widget)
{
    model()->setAdditionalItem(widget);
}

MWidget* MContentItem::additionalItem() const
{
    return model()->additionalItem();
}

void MContentItem::enableProgressBar()
{
    MProgressIndicator* progressIndicator = new MProgressIndicator;
    progressIndicator->setViewType(MProgressIndicator::barType);
    progressIndicator->setSizePolicy(QSizePolicy::MinimumExpanding,QSizePolicy::Expanding);
    model()->setAdditionalItem(progressIndicator);
}

void MContentItem::setSmallItem(MWidget* widget)
{
    Q_D(MContentItem);
    d->smallItem = widget;
}

MWidget* MContentItem::smallItem() const
{
    Q_D(const MContentItem);
    return d->smallItem;
}

void MContentItem::setSmallText(QString text)
{
    Q_D(MContentItem);
    MLabel* label = new MLabel(text);
    label->setAlignment( Qt::AlignRight );
    d->smallItem = label;
}

