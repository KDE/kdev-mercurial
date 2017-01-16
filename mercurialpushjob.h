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

#ifndef MERCURIALPUSHJOB_H
#define MERCURIALPUSHJOB_H

#include <vcs/vcsjob.h>

#include "mercurialplugin.h"

class MercurialPushJob : public KDevelop::VcsJob
{
    Q_OBJECT

public:
    MercurialPushJob(const QDir &workingDir, const QUrl &destination, MercurialPlugin *parent);
    void start() override;
    QVariant fetchResults() override;
    KDevelop::VcsJob::JobStatus status() const override;
    KDevelop::IPlugin *vcsPlugin() const override;

private slots:
    void serverContacted(KJob *job);

private:
    QDir m_workingDir;
    QUrl m_repoLocation;
    KDevelop::VcsJob::JobStatus m_status;

    void setFail();
    void setSuccess();
};

#endif // MERCURIALPUSHJOB_H
