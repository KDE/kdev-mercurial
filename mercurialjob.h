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

#pragma once

#include <vcs/vcsjob.h>

#include <QDir>
#include <QPointer>

class MercurialPlugin;

class MercurialJob : public KDevelop::VcsJob
{
public:
	MercurialJob(const QDir& workingDir, MercurialPlugin* parent, JobType jobType);

	QVariant fetchResults() override;
	KDevelop::VcsJob::JobStatus status() const override;
	KDevelop::IPlugin *vcsPlugin() const override;

protected:
	bool doKill() override;

	void setFail();
	void setSuccess();

	KDevelop::VcsJob::JobStatus m_status;
	mutable QPointer<KJob> m_job;
	QDir m_workingDir;
};
