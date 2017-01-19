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

#include "mercurialheadsmodel.h"

#include "../mercurialplugin.h"

#include <QIcon>

#include <interfaces/icore.h>
#include <interfaces/iruncontroller.h>

#include <vcs/vcsjob.h>
#include <vcs/vcsevent.h>
#include <vcs/vcsrevision.h>

using namespace KDevelop;

MercurialHeadsModel::MercurialHeadsModel(MercurialPlugin* plugin,  const QUrl& url, QObject* parent)
    : VcsBasicEventModel(parent)
    , m_updating(false)
    , m_plugin(plugin)
    , m_url(url)
{
}

QVariant MercurialHeadsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Vertical && role == Qt::DecorationRole) {
        const VcsRevision &rev = eventForIndex(index(section, 0)).revision();
        if (m_currentHeads.contains(rev)) {
            return QVariant(QIcon("arrow-right"));
        } else {
            return QVariant();
        }
    } else {
        return VcsBasicEventModel::headerData(section, orientation, role);
    }
}

void MercurialHeadsModel::update()
{
    VcsJob *identifyJob = m_plugin->identify(m_url);
    connect(identifyJob, &VcsJob::resultsReady, this, &MercurialHeadsModel::identifyReceived);
    ICore::self()->runController()->registerJob(identifyJob);

    VcsJob *headsJob = m_plugin->heads(m_url);
    connect(headsJob, &VcsJob::resultsReady, this, &MercurialHeadsModel::headsReceived);
    ICore::self()->runController()->registerJob(headsJob);
}

void MercurialHeadsModel::identifyReceived(VcsJob *job)
{
    QList<VcsRevision> currentHeads;
    foreach (const QVariant& value, job->fetchResults().toList()) {
        currentHeads << value.value<VcsRevision>();
    }
    m_currentHeads = currentHeads;
    if (m_updating) {
        emit updateComplete();
        m_updating = false;
    } else {
        m_updating = true;
    }
}

void MercurialHeadsModel::headsReceived(VcsJob *job)
{
    QList<VcsEvent> events;
    foreach (const QVariant& value, job->fetchResults().toList()) {
        events << value.value<VcsEvent>();
    }
    addEvents(events);
    if (m_updating) {
        emit updateComplete();
        m_updating = false;
    } else {
        m_updating = true;
    }
}
