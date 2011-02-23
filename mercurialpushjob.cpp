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
#include <QtGui/QMessageBox>
#include <KDebug>

using namespace KDevelop;

MercurialPushJob::MercurialPushJob(const QDir &workingDir, const KUrl &destination, MercurialPlugin *parent)
: VcsJob(parent), m_workingDir(workingDir), m_status(JobNotStarted)
{
    if (destination.isEmpty()) {
        m_repoLocation = static_cast<MercurialPlugin*>(vcsPlugin())->remotePushRepositoryLocation(m_workingDir);
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

    connect(job, SIGNAL(resultsReady(KDevelop::VcsJob*)), SLOT(serverContacted(KDevelop::VcsJob*)));
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

IPlugin* MercurialPushJob::vcsPlugin() const
{
    return static_cast<IPlugin*>(parent());
}

void MercurialPushJob::serverContacted(VcsJob *job)
{
    DVcsJob *dvcsJob = static_cast<DVcsJob*>(job);
    // check for errors
    if (dvcsJob->error()) {
        QString response = QString::fromLocal8Bit(dvcsJob->errorOutput());

        kDebug() << response;

        if (response.contains("abort: http authorization required")) {
            // server requests username:password auth -> ask pass
            KPasswordDialog dlg(0, KPasswordDialog::ShowUsernameLine);
            dlg.setPrompt(i18n("Enter your login and password for Mercurial push."));
            dlg.setUsername(m_repoLocation.user());
            dlg.setPassword(m_repoLocation.pass());
            if (!dlg.exec()) {
                setFail();
            } else {
                m_repoLocation.setUser(dlg.username());
                m_repoLocation.setPass(dlg.password());
                start();
            }
        } else if (response.contains("remote: Permission denied")) {
            // server is not happy about our ssh key -> nothing could be done via gui
            QMessageBox::critical(0, i18n("Mercurial push error"), i18n("Remote server does not accept your ssh key."));
        } else if (response.contains("remote: Host key verification failed.")) {
            // server key is not known for us
            // TODO: could be fixed via gui (SSH_ASKPASS etc)?
            QMessageBox::critical(0, i18n("Mercurial push error"), i18n("Remote server ssh fingerprint is unknown."));
        } else if (response.contains("abort: HTTP Error 404: NOT FOUND")) {
            // wrong url
            QMessageBox::critical(0, i18n("Mercurial push error"), i18n("Push URL is incorrect."));
        } else {
            // TODO
            QMessageBox::critical(0, i18n("Mercurial push error"), i18n("Unknown error while pushing. Please, check Version Control toolview."));
        }
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