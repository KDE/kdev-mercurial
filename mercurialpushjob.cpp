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

#include "mercurialpushjob.h"

#include <vcs/dvcs/dvcsjob.h>
#include <KPasswordDialog>
#include <KLocalizedString>

#include <QMessageBox>

#include "debug.h"

using namespace KDevelop;

MercurialPushJob::MercurialPushJob(const QDir &workingDir, const QUrl &destination, MercurialPlugin *parent)
    : VcsJob(parent), m_workingDir(workingDir), m_status(JobNotStarted)
{
    if (destination.isEmpty()) {
        m_repoLocation = static_cast<MercurialPlugin *>(vcsPlugin())->remotePushRepositoryLocation(m_workingDir);
    } else {
        m_repoLocation = destination;
    }
};

void MercurialPushJob::start()
{
    m_status = JobRunning;

    DVcsJob *job = new DVcsJob(m_workingDir, vcsPlugin());

    *job << "hg" << "push" << "--";

    if (!m_repoLocation.isEmpty())
        *job << m_repoLocation.url();

    connect(job, &KJob::finished, this, &MercurialPushJob::serverContacted);
    job->start();
}

QVariant MercurialPushJob::fetchResults()
{
    return QVariant();
}

VcsJob::JobStatus MercurialPushJob::status() const
{
    return m_status;
}

IPlugin *MercurialPushJob::vcsPlugin() const
{
    return static_cast<IPlugin *>(parent());
}

void MercurialPushJob::serverContacted(KJob *job)
{
    DVcsJob *dvcsJob = static_cast<DVcsJob *>(job);
    // check for errors
    if (dvcsJob->error()) {
        QString response = QString::fromLocal8Bit(dvcsJob->errorOutput());

        qCDebug(PLUGIN_MERCURIAL) << response;

        if (response.contains("abort: http authorization required")) {
            // server requests username:password auth -> ask pass
            KPasswordDialog dlg(nullptr, KPasswordDialog::ShowUsernameLine);
            dlg.setPrompt(i18n("Enter your login and password for Mercurial push."));
            dlg.setUsername(m_repoLocation.userName());
            dlg.setPassword(m_repoLocation.password());
            if (dlg.exec()) {
                m_repoLocation.setUserName(dlg.username());
                m_repoLocation.setPassword(dlg.password());
                return start();
            }
        } else if (response.contains("remote: Permission denied")) {
            // server is not happy about our ssh key -> nothing could be done via gui
            QMessageBox::critical(nullptr, i18n("Mercurial Push Error"), i18n("Remote server does not accept your SSH key."));
        } else if (response.contains("remote: Host key verification failed.")) {
            // server key is not known for us
            // TODO: could be fixed via gui (SSH_ASKPASS etc)?
            QMessageBox::critical(nullptr, i18n("Mercurial Push Error"), i18n("Remote server SSH fingerprint is unknown."));
        } else if (response.contains("abort: HTTP Error 404: NOT FOUND")) {
            // wrong url
            QMessageBox::critical(nullptr, i18n("Mercurial Push Error"), i18n("Push URL is incorrect."));
        } else {
            // TODO
            QMessageBox::critical(nullptr, i18n("Mercurial Push Error"), i18n("Unknown error while pushing. Please, check Version Control toolview."));
        }
        setFail();
    } else {
        setSuccess();
    }
}

void MercurialPushJob::setFail()
{
    m_status = JobFailed;
    emitResult();
    emit resultsReady(this);
}

void MercurialPushJob::setSuccess()
{
    m_status = JobSucceeded;
    emitResult();
    emit resultsReady(this);
}
