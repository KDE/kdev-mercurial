/*
    This file is part of KDevelop

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

#include "mercurialjob.h"

#include "mercurialplugin.h"

using namespace KDevelop;

MercurialJob::MercurialJob(const QDir& workingDir, MercurialPlugin* parent, JobType jobType)
    : VcsJob(parent, OutputJob::Silent),
    m_status(JobNotStarted),
    m_workingDir(workingDir)
{
    setType(jobType);
    setCapabilities(Killable);
}

QVariant MercurialJob::fetchResults()
{
    return {};
}

VcsJob::JobStatus MercurialJob::status() const
{
    return m_status;
}

KDevelop::IPlugin* MercurialJob::vcsPlugin() const
{
    return static_cast<IPlugin *>(parent());
}

bool MercurialJob::doKill()
{
    m_status = JobCanceled;
    if (m_job) {
        return m_job->kill(KJob::Quietly);
    }
    return true;
}


void MercurialJob::setFail()
{
    m_status = JobFailed;
    emitResult();
    emit resultsReady(this);
}


void MercurialJob::setSuccess()
{
    m_status = JobSucceeded;
    emitResult();
    emit resultsReady(this);
}
