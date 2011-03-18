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

#include "mlocalebuckets.h"
#include "mlocalebuckets_p.h"


MLocaleBucketsPrivate::MLocaleBucketsPrivate() :
    locale()
#ifdef HAVE_ICU
    , collator(locale)
#endif
{
#ifdef HAVE_ICU
    allBuckets = locale.exemplarCharactersIndex();
#endif
}

void MLocaleBucketsPrivate::setItems(const QStringList &unsortedItems, Qt::SortOrder sortOrder)
{
    // Remember to call clear() first if this is called from somewhere else than a constructor!
    QList<MLocaleBucketItem> items;

    for (int i=0; i < unsortedItems.size(); ++i) {
        items.append(MLocaleBucketItem(unsortedItems.at(i), i));
    }
    qStableSort(items.begin(), items.end(), MLocaleBucketItemComparator(sortOrder));

    QString lastBucket;
    QStringList lastBucketItems;
    QList<int>  lastBucketOrigIndices;

    foreach (MLocaleBucketItem item, items) {

#ifdef HAVE_ICU
        QString bucket = locale.indexBucket(item.text, allBuckets, collator);
#else
        // Simplistic fallback if there is no libICU: Use the first character
        QString bucket = item.text.isEmpty() ? "" : QString(item.text[0]);
#endif
        if (bucket != lastBucket) {
            if (!lastBucketItems.isEmpty()) {
                // Found a new bucket - store away the old one
                buckets << lastBucket;
                bucketItems << lastBucketItems;
                origIndices << lastBucketOrigIndices;
                lastBucketItems.clear();
                lastBucketOrigIndices.clear();
            }
            lastBucket = bucket;
        }
        lastBucketItems << item.text;
        lastBucketOrigIndices << item.origIndex;
    }

    if (!lastBucketItems.isEmpty()) {
        buckets << lastBucket;
        bucketItems << lastBucketItems;
        origIndices << lastBucketOrigIndices;
    }
}

void MLocaleBucketsPrivate::clear()
{
    buckets.clear();
    bucketItems.clear();
    origIndices.clear();
}



MLocaleBuckets::MLocaleBuckets():
    d_ptr(new MLocaleBucketsPrivate())
{
    Q_D(MLocaleBuckets);

    d->q_ptr = this;
}

MLocaleBuckets::MLocaleBuckets(const QStringList &unsortedItems, Qt::SortOrder sortOrder) :
    d_ptr(new MLocaleBucketsPrivate())
{
    Q_D(MLocaleBuckets);

    d->q_ptr = this;
    d->setItems(unsortedItems, sortOrder);
}

MLocaleBuckets::~MLocaleBuckets()
{
    Q_D(MLocaleBuckets);

    delete d;
}

void MLocaleBuckets::setItems(const QStringList &items, Qt::SortOrder sortOrder)
{
    Q_D(MLocaleBuckets);

    d->clear();
    d->setItems(items, sortOrder);
}

int MLocaleBuckets::bucketCount() const
{
    Q_D(const MLocaleBuckets);

    return d->buckets.count();
}

QString MLocaleBuckets::bucketName(int bucketIndex) const
{
    Q_D(const MLocaleBuckets);

    if (bucketIndex < 0 || bucketIndex >= d->buckets.size())
        return QString();
    else
        return d->buckets.at(bucketIndex);
}

QString MLocaleBuckets::bucketName(const QString &item) const
{
#ifdef HAVE_ICU
    Q_D(const MLocaleBuckets);
    return d->locale.indexBucket(item, d->allBuckets, d->collator);
#else
    return item.isEmpty() ? "" : QString(item[0]);
#endif
}

int MLocaleBuckets::bucketIndex(const QString &bucketName) const
{
    Q_D(const MLocaleBuckets);

    return d->buckets.indexOf(bucketName);
}

QStringList MLocaleBuckets::bucketContent(int bucketIndex) const
{
    Q_D(const MLocaleBuckets);

    if (bucketIndex < 0 || bucketIndex >= d->buckets.size())
        return QStringList();
    else
        return d->bucketItems.at(bucketIndex);
}

int MLocaleBuckets::origItemIndex(int bucketIndex, int indexInBucket) const
{
    Q_D(const MLocaleBuckets);

    if (bucketIndex >= 0 && bucketIndex < d->buckets.size()) {
        const QList<int> &origIndices = d->origIndices.at(bucketIndex);
        if (indexInBucket >= 0 && indexInBucket < origIndices.size()) {
            return origIndices.at(indexInBucket);
        }
    }
    return -1;
}

int MLocaleBuckets::bucketSize(int bucketIndex) const
{
    Q_D(const MLocaleBuckets);

    if (bucketIndex < 0 || bucketIndex >= d->buckets.size())
        return 0;
    else
        return d->bucketItems.at(bucketIndex).size();
}

bool MLocaleBuckets::isEmpty() const
{
    Q_D(const MLocaleBuckets);

    return d->bucketItems.isEmpty();
}

void MLocaleBuckets::clear()
{
    Q_D(MLocaleBuckets);

    d->clear();
}