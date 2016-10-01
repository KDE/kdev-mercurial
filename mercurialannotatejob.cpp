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

namespace {
QString revisionToString(const KDevelop::VcsRevision& rev)
{
    if (rev.revisionType() == KDevelop::VcsRevision::Special) {
        if (rev.specialType() == KDevelop::VcsRevision::Head) {
            return QStringLiteral("tip");
        } else if (rev.specialType() == KDevelop::VcsRevision::Start) {
            return QStringLiteral("0");
        }
    } else if (rev.revisionType() == KDevelop::VcsRevision::GlobalNumber) {
        return QString::number(rev.revisionValue().toLongLong());
    }

    return {};
}
}

MercurialAnnotateJob::MercurialAnnotateJob(const QDir &workingDir, const VcsRevision& revision, const QUrl& location, MercurialPlugin *parent)
    : VcsJob(parent),
    m_workingDir(workingDir),
    m_revision(revision),
    m_location(location),
    m_status(JobNotStarted)
{
    setType(JobType::Annotate);
    setCapabilities(Killable);
}

void MercurialAnnotateJob::start()
{
    m_status = JobRunning;

    DVcsJob *job = new DVcsJob(m_workingDir, vcsPlugin());
    *job << "hg" << "annotate" << "-n" << "-d";

    *job << "--" << m_location.toLocalFile();

    connect(job, SIGNAL(resultsReady(KDevelop::VcsJob *)), SLOT(parseAnnotateOutput(KDevelop::VcsJob *)));
    job->start();
}

QVariant MercurialAnnotateJob::fetchResults()
{
    return m_annotations;
}

VcsJob::JobStatus MercurialAnnotateJob::status() const
{
    return m_status;
}

IPlugin *MercurialAnnotateJob::vcsPlugin() const
{
    return static_cast<IPlugin *>(parent());
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
            mercurialDebug() << "Could not parse annotation line: \"" << line << '\"';
            return setFail();
        }
        VcsAnnotationLine annotation;
        annotation.setLineNumber(lineNumber++);
        annotation.setText(reAnnot.cap(3));

        bool success = false;
        qlonglong rev = reAnnot.cap(1).toLongLong(&success);
        if (!success) {
            mercurialDebug() << "Could not parse revision in annotation line: \"" << line << '\"';
            return setFail();
        }

        QDateTime dt = QDateTime::fromString(reAnnot.cap(2).left(reAnnot.cap(2).lastIndexOf(" ")), "ddd MMM dd hh:mm:ss yyyy");
        //mercurialDebug() << reAnnot.cap(2).left(reAnnot.cap(2).lastIndexOf(" ")) << dt;
        Q_ASSERT(dt.isValid());
        annotation.setDate(dt);

        VcsRevision vcsrev;
        vcsrev.setRevisionValue(rev, VcsRevision::GlobalNumber);
        annotation.setRevision(vcsrev);
        m_annotations.push_back(qVariantFromValue(annotation));
    }

    for (const auto& annotation: m_annotations) {
        const auto revision = revisionToString(annotation.value<VcsAnnotationLine>().revision());
        if (!m_revisionsToLog.contains(revision)) {
            m_revisionsToLog.insert(revision);
        }
    }

    nextPartOfLog();
}

void MercurialAnnotateJob::nextPartOfLog()
{
    m_status = JobRunning;

    DVcsJob *logJob = new DVcsJob(m_workingDir, vcsPlugin());

    *logJob << "hg" << "log" << "--template" << "{rev}\\_%{desc|firstline}\\_%{author}\\_%";

    int numRevisions = 0;
    for (auto it = m_revisionsToLog.begin(); it != m_revisionsToLog.end();) {
        *logJob << "-r" << *it;
        it = m_revisionsToLog.erase(it);

        if (++numRevisions >= 200) {
            // Prevent mercurial crash
            break;
        }
    }

    connect(logJob, SIGNAL(resultsReady(KDevelop::VcsJob *)), SLOT(parseLogOutput(KDevelop::VcsJob *)));
    logJob->start();

}

void MercurialAnnotateJob::parseLogOutput(KDevelop::VcsJob* j)
{
    DVcsJob *job = static_cast<DVcsJob *>(j);

    auto items = job->output().split("\\_%");
    items.removeLast();

    if (items.size() % 3) {
        mercurialDebug() << "Cannot parse annotate log: unexpected number of entries" << items.size() << m_annotations.size();
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
        const auto revision = revisionToString(annotationLine.revision());

        if (!m_revisionsCache.contains(revision)) {
            Q_ASSERT(m_revisionsCache.contains(revision));
            continue;
        }
        mercurialDebug() << m_revisionsCache.value(revision).first << m_revisionsCache.value(revision).second;
        if (m_revisionsCache.value(revision).first.isEmpty() && m_revisionsCache.value(revision).second.isEmpty()) {
            mercurialDebug() << "Strange revision" << revision;
            continue;
        }
        annotationLine.setCommitMessage(m_revisionsCache.value(revision).first);
        annotationLine.setAuthor(m_revisionsCache.value(revision).second);
        m_annotations[idx] = qVariantFromValue(annotationLine);
    }

    setSuccess();
}

void MercurialAnnotateJob::setFail()
{
    m_status = JobFailed;
    emitResult();
    emit resultsReady(this);
}

void MercurialAnnotateJob::setSuccess()
{
    m_status = JobSucceeded;
    emitResult();
    emit resultsReady(this);
}
