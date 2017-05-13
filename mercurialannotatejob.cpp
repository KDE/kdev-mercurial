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

#include "mercurialannotatejob.h"

#include <vcs/vcsannotation.h>
#include <vcs/dvcs/dvcsjob.h>

#include <QDateTime>

#include "debug.h"

using namespace KDevelop;

MercurialAnnotateJob::MercurialAnnotateJob(const QDir &workingDir, const VcsRevision& revision, const QUrl& location, MercurialPlugin *parent)
    : MercurialJob(workingDir, parent, JobType::Annotate),
    m_revision(revision),
    m_location(location)
{}

void MercurialAnnotateJob::start()
{
    m_status = JobRunning;

    DVcsJob *job = new DVcsJob(m_workingDir, vcsPlugin(), KDevelop::OutputJob::Silent);
    if (Q_UNLIKELY(m_testCase == TestCase::Status)) {
         *job << "some_command_that_doesnt_exist";
    } else {
        *job << "hg" << "status" << "-m"  << "-a" << "-n";

        *job << "--" << m_location.toLocalFile();
    }

    connect(job, &DVcsJob::resultsReady, this, &MercurialAnnotateJob::parseStatusResult);
    connect(job, &KJob::finished, this, &MercurialAnnotateJob::subJobFinished);
    m_job = job;
    job->start();
}

void MercurialAnnotateJob::parseCommitResult(KDevelop::VcsJob* j)
{
    DVcsJob *job = static_cast<DVcsJob *>(j);
    if (job->status() != VcsJob::JobSucceeded)
        return setFail();

    launchAnnotateJob();
}

void MercurialAnnotateJob::launchAnnotateJob() const
{
    DVcsJob *annotateJob = new DVcsJob(m_workingDir, vcsPlugin(), KDevelop::OutputJob::Silent);
    if (Q_UNLIKELY(m_testCase == TestCase::Annotate)) {
         *annotateJob << "some_command_that_doesnt_exist";
    } else {
        *annotateJob << "hg" << "annotate" << "-n" << "-d";

        *annotateJob << "--" << m_location.toLocalFile();
    }

    connect(annotateJob, &DVcsJob::resultsReady, this, &MercurialAnnotateJob::parseAnnotateOutput);
    connect(annotateJob, &KJob::finished, this, &MercurialAnnotateJob::subJobFinished);
    m_job = annotateJob;
    annotateJob->start();
}

void MercurialAnnotateJob::parseStatusResult(KDevelop::VcsJob* j)
{
    DVcsJob *job = static_cast<DVcsJob *>(j);
    if (job->status() != VcsJob::JobSucceeded)
        return setFail();

    if (job->output().isEmpty()) {
        launchAnnotateJob();
    } else {
        m_hasModifiedFile = true;
        DVcsJob *commitJob = new DVcsJob(m_workingDir, vcsPlugin(), KDevelop::OutputJob::Silent);
        if (Q_UNLIKELY(m_testCase == TestCase::Commit)) {
            *commitJob << "some_command_that_doesnt_exist";
        } else {
            *commitJob << "hg" << "commit" << "-u"  << "not.committed.yet" << "-m" << "Not Committed Yet";

            *commitJob << "--" << m_location.toLocalFile();
        }
        connect(commitJob, &DVcsJob::resultsReady, this, &MercurialAnnotateJob::parseCommitResult);
        connect(commitJob, &KJob::finished, this, &MercurialAnnotateJob::subJobFinished);
        m_job = commitJob;
        commitJob->start();
    }
}

void MercurialAnnotateJob::parseStripResult(KDevelop::VcsJob* /*job*/)
{
    setSuccess();
}

QVariant MercurialAnnotateJob::fetchResults()
{
    return m_annotations;
}

void MercurialAnnotateJob::parseAnnotateOutput(VcsJob *j)
{
    DVcsJob *job = static_cast<DVcsJob *>(j);
    if (job->status() != VcsJob::JobSucceeded)
        return setFail();

    QStringList lines = job->output().split('\n');
    lines.removeLast();

    static const QString reAnnotPat("\\s*(\\d+)\\s+(\\w+ \\w+ \\d\\d \\d\\d:\\d\\d:\\d\\d \\d\\d\\d\\d .\\d\\d\\d\\d): ([^\n]*)");
    QRegExp reAnnot(reAnnotPat, Qt::CaseSensitive, QRegExp::RegExp2);
    unsigned int lineNumber = 0;
    foreach (const QString& line, lines) {
        if (!reAnnot.exactMatch(line)) {
            qCDebug(PLUGIN_MERCURIAL) << "Could not parse annotation line: \"" << line << '\"';
            return setFail();
        }
        VcsAnnotationLine annotation;
        annotation.setLineNumber(lineNumber++);
        annotation.setText(reAnnot.cap(3));

        bool success = false;
        qlonglong rev = reAnnot.cap(1).toLongLong(&success);
        if (!success) {
            qCDebug(PLUGIN_MERCURIAL) << "Could not parse revision in annotation line: \"" << line << '\"';
            return setFail();
        }

        QDateTime dt = QDateTime::fromString(reAnnot.cap(2).left(reAnnot.cap(2).lastIndexOf(" ")), "ddd MMM dd hh:mm:ss yyyy");
        //qCDebug(PLUGIN_MERCURIAL) << reAnnot.cap(2).left(reAnnot.cap(2).lastIndexOf(" ")) << dt;
        Q_ASSERT(dt.isValid());
        annotation.setDate(dt);

        VcsRevision vcsrev;
        vcsrev.setRevisionValue(rev, VcsRevision::GlobalNumber);
        annotation.setRevision(vcsrev);
        m_annotations.push_back(qVariantFromValue(annotation));
    }

    for (const auto& annotation: m_annotations) {
        const auto revision = static_cast<MercurialPlugin*>(vcsPlugin())->toMercurialRevision(annotation.value<VcsAnnotationLine>().revision());
        if (!m_revisionsToLog.contains(revision)) {
            m_revisionsToLog.insert(revision);
        }
    }

    nextPartOfLog();
}

void MercurialAnnotateJob::nextPartOfLog()
{
    m_status = JobRunning;

    DVcsJob *logJob = new DVcsJob(m_workingDir, vcsPlugin(), KDevelop::OutputJob::Silent);
    if (Q_UNLIKELY(m_testCase == TestCase::Log)) {
         *logJob << "some_command_that_doesnt_exist";
    } else {
        *logJob << "hg" << "log" << "--template" << "{rev}\\_%{desc|firstline}\\_%{author}\\_%";
    }

    int numRevisions = 0;
    for (auto it = m_revisionsToLog.begin(); it != m_revisionsToLog.end();) {
        *logJob << "-r" << *it;
        it = m_revisionsToLog.erase(it);

        if (++numRevisions >= 200) {
            // Prevent mercurial crash
            break;
        }
    }

    connect(logJob, &DVcsJob::resultsReady, this, &MercurialAnnotateJob::parseLogOutput);
    connect(logJob, &KJob::finished, this, &MercurialAnnotateJob::subJobFinished);
    m_job = logJob;
    logJob->start();
}

void MercurialAnnotateJob::parseLogOutput(KDevelop::VcsJob* j)
{
    DVcsJob *job = static_cast<DVcsJob *>(j);

    auto items = job->output().split("\\_%");
    items.removeLast();

    if (items.size() % 3) {
        qCDebug(PLUGIN_MERCURIAL) << "Cannot parse annotate log: unexpected number of entries" << items.size() << m_annotations.size();
        return setFail();
    }

    for (auto it = items.constBegin(); it != items.constEnd(); it += 3) {
        if (!m_revisionsCache.contains(*it)) {
            m_revisionsCache[*it] = {*(it + 1), *(it + 2)};
        }
    }

    if (!m_revisionsToLog.isEmpty()) {
        // TODO: We can show what we've got so far
        return nextPartOfLog();
    }

    for (int idx = 0; idx < m_annotations.size(); idx++) {
        auto annotationLine = m_annotations[idx].value<VcsAnnotationLine>();
        const auto revision = static_cast<MercurialPlugin*>(vcsPlugin())->toMercurialRevision(annotationLine.revision());

        if (!m_revisionsCache.contains(revision)) {
            Q_ASSERT(m_revisionsCache.contains(revision));
            continue;
        }
        qCDebug(PLUGIN_MERCURIAL) << m_revisionsCache.value(revision).first << m_revisionsCache.value(revision).second;
        if (m_revisionsCache.value(revision).first.isEmpty() && m_revisionsCache.value(revision).second.isEmpty()) {
            qCDebug(PLUGIN_MERCURIAL) << "Strange revision" << revision;
            continue;
        }
        annotationLine.setCommitMessage(m_revisionsCache.value(revision).first);
        annotationLine.setAuthor(m_revisionsCache.value(revision).second);
        m_annotations[idx] = qVariantFromValue(annotationLine);
    }

    if (m_hasModifiedFile) {
        DVcsJob* stripJob = new DVcsJob(m_workingDir, vcsPlugin(), KDevelop::OutputJob::Silent);
        if (Q_UNLIKELY(m_testCase == TestCase::Strip)) {
            *stripJob << "some_command_that_doesnt_exist";
        } else {
            *stripJob << "hg" << "--config" << "extensions.mq=" << "strip" << "-k" << "tip";
        }

        connect(stripJob, &DVcsJob::resultsReady, this, &MercurialAnnotateJob::parseStripResult);
        connect(stripJob, &KJob::finished, this, &MercurialAnnotateJob::subJobFinished);
        m_job = stripJob;
        stripJob->start();
    } else {
        setSuccess();
    }
}

void MercurialAnnotateJob::subJobFinished(KJob* job)
{
    if (job->error()) {
        setFail();
    }
}
