/*
    This file is part of KDevelop

    Copyright 2016 Sergey Kalinichev <kalinichev.so.0@gmail.com>

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

#include "mercurialplugin.h"

#include <QSet>

class MercurialAnnotateJob : public KDevelop::VcsJob
{
    Q_OBJECT

public:
    MercurialAnnotateJob(const QDir &workingDir, const KDevelop::VcsRevision& revision, const QUrl& location, MercurialPlugin *parent);
    void start() override;
    QVariant fetchResults() override;
    KDevelop::VcsJob::JobStatus status() const override;
    KDevelop::IPlugin *vcsPlugin() const override;

protected:
    bool doKill() override;

private slots:
    void parseAnnotateOutput(KDevelop::VcsJob *job);
    void parseLogOutput(KDevelop::VcsJob *job);
    void parseCommitResult(KDevelop::VcsJob *job);
    void parseStripResult(KDevelop::VcsJob *job);
    void parseStatusResult(KDevelop::VcsJob *job);
    void subJobFinished(KJob *job);

private:
    void launchAnnotateJob() const;

    QDir m_workingDir;
    KDevelop::VcsRevision m_revision;
    QUrl m_location;
    KDevelop::VcsJob::JobStatus m_status;
    mutable QPointer<KJob> m_job;
    QList<QVariant> m_annotations;
    QHash<QString, QPair<QString, QString>> m_revisionsCache;
    QSet <QString> m_revisionsToLog;
    bool m_hasModifiedFile = false;

    void setFail();
    void setSuccess();
    void nextPartOfLog();

    // Those variables below for testing purposes only
    friend class MercurialTest;
    enum class TestCase
    {
      None,
      Status,
      Commit,
      Annotate,
      Log,
      Strip
    };
    TestCase m_testCase = TestCase::None;
};
