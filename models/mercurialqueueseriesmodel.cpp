/***************************************************************************
 *   Copyright 2011 Andrey Batyiev <batyiev@gmail.com>                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include <QStringList>
#include <QIcon>

#include <mercurialplugin.h>
#include "mercurialqueueseriesmodel.h"
#include <vcs/vcsjob.h>

using namespace KDevelop;

void MercurialQueueSeriesModel::update()
{
    beginResetModel();
    VcsJob *job = m_plugin->mqApplied(m_repoLocation);
    if (job->exec() && job->status() == VcsJob::JobSucceeded) {
        m_appliedPatches = job->fetchResults().toStringList();
    }
    delete job;

    job = m_plugin->mqUnapplied(m_repoLocation);
    if (job->exec() && job->status() == VcsJob::JobSucceeded) {
        m_unappliedPatches = job->fetchResults().toStringList();
    }
    delete job;
    endResetModel();
}

QVariant MercurialQueueSeriesModel::data(const QModelIndex &index, int role) const
{
    int row = index.row();
    if (row >= 0 && row < m_appliedPatches.size()) {
        if (role == Qt::DisplayRole) {
            return m_appliedPatches[row];
        } else if (role == Qt::DecorationRole) {
            // FIXME
            return QVariant(QIcon("dialog-ok-apply"));
        } else {
            return QVariant();
        }
    } else {
        row -= m_appliedPatches.size();
    }

    if (row >= 0 && row < m_unappliedPatches.size()) {
        if (role == Qt::DisplayRole) {
            return m_unappliedPatches[row];
        } else {
            return QVariant();
        }
    }
    return QVariant();
}

int MercurialQueueSeriesModel::rowCount(const QModelIndex &/*parent*/) const
{
    return m_unappliedPatches.size() + m_appliedPatches.size();
}
