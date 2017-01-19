/*
    This file is part of KDevelop

    Copyright 2011 Andrey Batyiev <batyiev@gmail.com>
    Copyright 2017 Sergey Kalinichev <kalinichev.so.0@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef MERCURIALHEADSMODEL_H
#define MERCURIALHEADSMODEL_H

#include "vcs/models/vcseventmodel.h"

#include <QUrl>

namespace KDevelop
{
    class VcsJob;
}

class MercurialPlugin;

class MercurialHeadsModel : public KDevelop::VcsBasicEventModel
{
    Q_OBJECT
public:
    MercurialHeadsModel(MercurialPlugin* plugin, const QUrl& url, QObject* parent);
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void update();

signals:
    void updateComplete();

private slots:
    void headsReceived(KDevelop::VcsJob* job);
    void identifyReceived(KDevelop::VcsJob* job);

private:
    bool m_updating;
    MercurialPlugin* m_plugin;
    QUrl m_url;
    QList<KDevelop::VcsRevision> m_currentHeads;
};

#endif // MERCURIALHEADSMODEL_H
