/***************************************************************************
 *   This file was taken from KDevelop's git plugin                        *
 *   Copyright 2008 Evgeniy Ivanov <powerfox@kde.ru>                       *
 *                                                                         *
 *   Adapted for Mercurial                                                 *
 *   Copyright 2009 Fabian Wiesel <fabian.wiesel@fu-berlin.de>             *
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

#include "mercurialplugin.h"

#include <algorithm>
#include <memory>

#include <QtCore/QDir>
#include <QtCore/QDateTime>
#include <QtCore/QFileInfo>
#include <QtCore/QLoggingCategory>
#include <QAction>
#include <QMenu>
#include <QDateTime>

#include <interfaces/icore.h>

#include <vcs/vcsjob.h>
#include <vcs/vcsevent.h>
#include <vcs/vcsrevision.h>
#include <vcs/vcsannotation.h>
#include <vcs/dvcs/dvcsjob.h>

#include "mercurialvcslocationwidget.h"
#include "mercurialpushjob.h"
#include "ui/mercurialheadswidget.h"
#include "ui/mercurialqueuesmanager.h"

#include "debug.h"

Q_LOGGING_CATEGORY(PLUGIN_MERCURIAL, "kdevplatform.plugins.mercurial")

using namespace KDevelop;

MercurialPlugin::MercurialPlugin(QObject *parent, const QVariantList &)
    : DistributedVersionControlPlugin(parent, QStringLiteral("kdevmercurial"))
{
    KDEV_USE_EXTENSION_INTERFACE(KDevelop::IBasicVersionControl)
    KDEV_USE_EXTENSION_INTERFACE(KDevelop::IDistributedVersionControl)

     // FIXME:
    /*
    m_headsAction = new KAction(i18n("Heads..."), this);
    m_mqNew = new KAction(i18nc("mercurial queues submenu", "New..."), this);
    m_mqPushAction = new KAction(i18nc("mercurial queues submenu", "Push"), this);
    m_mqPushAllAction = new KAction(i18nc("mercurial queues submenu", "Push All"), this);
    m_mqPopAction = new KAction(i18nc("mercurial queues submenu", "Pop"), this);
    m_mqPopAllAction = new KAction(i18nc("mercurial queues submenu", "Pop All"), this);
    m_mqManagerAction = new KAction(i18nc("mercurial queues submenu", "Manager..."), this);*/

   
    /*
    connect(m_headsAction, SIGNAL(triggered()), this, SLOT(showHeads()));
    connect(m_mqManagerAction, SIGNAL(triggered()), this, SLOT(showMercurialQueuesManager()));*/
}

MercurialPlugin::~MercurialPlugin()
{
}

QString MercurialPlugin::name() const
{
    return QLatin1String("Mercurial");
}

bool MercurialPlugin::isValidDirectory(const QUrl &directory)
{
    // Mercurial uses the same test, so we don't lose any functionality
    static const QString hgDir(".hg");

    if (m_lastRepoRoot.isParentOf(directory))
        return true;

    const QString initialPath(directory.toLocalFile());
    const QFileInfo finfo(initialPath);
    QDir dir;
    if (finfo.isFile()) {
        dir = finfo.absoluteDir();
    } else {
        dir = QDir(initialPath);
        dir.makeAbsolute();
    }

    while (!dir.cd(hgDir) && dir.cdUp())
    {} // cdUp, until there is a sub-directory called .hg

    if (hgDir != dir.dirName())
        return false;

    dir.cdUp(); // Leave .hg
    m_lastRepoRoot.setPath(dir.absolutePath());
    return true;
}

bool MercurialPlugin::isVersionControlled(const QUrl &url)
{
    const QFileInfo fsObject(url.toLocalFile());

    if (!fsObject.isFile()) {
        return isValidDirectory(url);
    }

    DVcsJob *job = static_cast<DVcsJob *>(status({url}, Recursive));
    if (!job->exec()) {
        return false;
    }

    QList<QVariant> statuses = qvariant_cast<QList<QVariant> >(job->fetchResults());
    VcsStatusInfo info = qvariant_cast< VcsStatusInfo >(statuses.first());
    if (info.state() == VcsStatusInfo::ItemAdded ||
            info.state() == VcsStatusInfo::ItemModified ||
            info.state() == VcsStatusInfo::ItemUpToDate) {
        return true;
    }

    return false;
}

VcsJob *MercurialPlugin::init(const QUrl &directory)
{
    DVcsJob *job = new DVcsJob(directory.path(), this);

    *job << "hg" << "init";

    return job;
}

VcsJob *MercurialPlugin::repositoryLocation(const QUrl &directory)
{
    return NULL;
}

VcsJob *MercurialPlugin::createWorkingCopy(const VcsLocation &localOrRepoLocationSrc, const QUrl &destinationDirectory, IBasicVersionControl::RecursionMode)
{
    DVcsJob *job = new DVcsJob(QDir::home(), this);

    *job << "hg" << "clone" << "--" << localOrRepoLocationSrc.localUrl().toLocalFile() << destinationDirectory.toLocalFile();

    return job;
}

VcsJob *MercurialPlugin::pull(const VcsLocation &otherRepository, const QUrl &workingRepository)
{
    DVcsJob *job = new DVcsJob(workingRepository.toLocalFile(), this);

    *job << "hg" << "pull" << "--";

    QString pathOrUrl = otherRepository.localUrl().toLocalFile();

    if (!pathOrUrl.isEmpty())
        *job << pathOrUrl;

    return job;
}

VcsJob *MercurialPlugin::push(const QUrl &workingRepository, const VcsLocation &otherRepository)
{
    return new MercurialPushJob(findWorkingDir(workingRepository), otherRepository.localUrl(), this);
}

VcsJob *MercurialPlugin::add(const QList<QUrl> &localLocations, IBasicVersionControl::RecursionMode recursion)
{
    QList<QUrl> locations = localLocations;

    if (recursion == NonRecursive) {
        filterOutDirectories(locations);
    }

    if (localLocations.empty()) {
        // nothing left after filtering
        return NULL;
    }

    DVcsJob *job = new DVcsJob(findWorkingDir(locations.first()), this);
    *job << "hg" << "add" << "--" << locations;
    return job;
}

VcsJob *MercurialPlugin::copy(const QUrl &localLocationSrc, const QUrl &localLocationDst)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocationSrc), this);
    *job << "hg" << "cp" << "--" << localLocationSrc.toLocalFile() << localLocationDst.path();
    return job;
}

VcsJob *MercurialPlugin::move(const QUrl &localLocationSrc,
                              const QUrl &localLocationDst)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocationSrc), this);
    *job << "hg" << "mv" << "--" << localLocationSrc.toLocalFile() << localLocationDst.path();
    return job;
}

//If no files specified then commit already added files
VcsJob *MercurialPlugin::commit(const QString &message,
                                const QList<QUrl> &localLocations,
                                IBasicVersionControl::RecursionMode recursion)
{
    QList<QUrl> locations = localLocations;

    if (recursion == NonRecursive) {
        filterOutDirectories(locations);
    }

    if (locations.empty() || message.isEmpty()) {
        return NULL;
    }

    DVcsJob *job = new DVcsJob(findWorkingDir(locations.first()), this);
    *job << "hg" << "commit" << "-m" << message << "--" << locations;
    return job;
}

VcsJob *MercurialPlugin::update(const QList<QUrl> &localLocations,
                                const VcsRevision &rev,
                                IBasicVersionControl::RecursionMode recursion)
{
    /* TODO: update to Head != pull, but for consistency with git plugin...
     * TODO: per file pulls?
     * TODO: why rev == VcsRevision::createSpecialRevision(VcsRevision::Head) doesn't work?
     */
    if (rev.revisionType() == VcsRevision::Special &&
            rev.revisionValue().value<VcsRevision::RevisionSpecialType>() == VcsRevision::Head) {
        return pull(VcsLocation(), findWorkingDir(localLocations.first()).path());
    }

    /*
     * actually reverting files
     * TODO: hg revert does not change parents, hg update does
     *       hg revert works on files, hg update doesn't
     * maybe do hg update only when top dir update() is requested?
     * TODO: nobody calls update with something other than VcsRevision::Head
     */
    QList<QUrl> locations = localLocations;

    if (recursion == NonRecursive) {
        filterOutDirectories(locations);
    }

    if (locations.empty()) {
        return NULL;
    }

    DVcsJob *job = new DVcsJob(findWorkingDir(locations.first()), this);

    //Note: the message is quoted somewhere else, so if we quote here then we have quotes in the commit log
    // idk what does previous comment mean -- abatyiev
    *job << "hg" << "revert" << "-r" << toMercurialRevision(rev) << "--" << locations;
    return job;
}

VcsJob *MercurialPlugin::resolve(const QList<QUrl>   &files, KDevelop::IBasicVersionControl::RecursionMode recursion)
{
    QList<QUrl> fileList = files;
    if (recursion == NonRecursive) {
        filterOutDirectories(fileList);
    }

    if (fileList.empty()) {
        return NULL;
    }

    DVcsJob *job = new DVcsJob(findWorkingDir(fileList.first()), this);

    //Note: the message is quoted somewhere else, so if we quote here then we have quotes in the commit log
    *job << "hg" << "resolve" << "--" << fileList;

    return job;
}

VcsJob *MercurialPlugin::diff(const QUrl &fileOrDirectory,
                              const VcsRevision &srcRevision,
                              const VcsRevision &dstRevision,
                              VcsDiff::Type diffType,
                              IBasicVersionControl::RecursionMode recursionMode)
{
    if (!fileOrDirectory.isLocalFile()) {
        return NULL;
    }

    // non-standard searching for working directory
    QString workingDir;
    QFileInfo fileInfo(fileOrDirectory.toLocalFile());

    // It's impossible to non-recursively diff directories
    if (recursionMode == NonRecursive && fileInfo.isDir()) {
        return NULL;
    }

    // find out correct working directory
    if (fileInfo.isFile()) {
        workingDir = fileInfo.absolutePath();
    } else {
        workingDir = fileInfo.absoluteFilePath();
    }

    QString srcRev = toMercurialRevision(srcRevision);
    QString dstRev = toMercurialRevision(dstRevision);

    mercurialDebug() << "Diff between" << srcRevision.prettyValue() << '(' << srcRev << ')' << "and" << dstRevision.prettyValue() << '(' << dstRev << ')' << " requested";

    if (
        (srcRev.isNull() && dstRev.isNull()) ||
        (srcRev.isEmpty() && dstRev.isEmpty())
    ) {
        qWarning() << "Diff between" << srcRevision.prettyValue() << '(' << srcRev << ')' << "and" << dstRevision.prettyValue() << '(' << dstRev << ')' << " not possible";
        return NULL;
    }

    const QString srcPath = fileOrDirectory.toLocalFile();

    DVcsJob *job = new DVcsJob(workingDir, this);

    *job << "hg" << "diff" << "-g";

    // Hg cannot do a diff from
    //   SomeRevision to Previous, or
    //   Working to SomeRevsion directly, but the reverse
    if (dstRev.isNull() // Destination is "Previous"
            || (!srcRev.isNull() && srcRev.isEmpty()) // Source is "Working"
       ) {
        std::swap(srcRev, dstRev);
        *job << "--reverse";
    }

    if (diffType == VcsDiff::DiffUnified) {
        *job << "-U" << "3";    // Default from GNU diff
    }

    if (srcRev.isNull() /* from "Previous" */ && dstRev.isEmpty() /* to "Working" */) {
        // Do nothing, that is the default
    } else if (srcRev.isNull()) { // Changes made in one arbitrary revision
        *job << "-c" << dstRev;
    } else {
        *job << "-r" << srcRev;
        if (!dstRev.isEmpty()) {
            *job << "-r" << dstRev;
        }
    }

    *job << "--" << srcPath;

    connect(job, SIGNAL(readyForParsing(KDevelop::DVcsJob *)), SLOT(parseDiff(KDevelop::DVcsJob *)));

    return job;
}

VcsJob *MercurialPlugin::remove(const QList<QUrl> &files)
{
    if (files.empty()) {
        return NULL;
    }

    DVcsJob *job = new DVcsJob(findWorkingDir(files.first()), this);
    *job << "hg" << "rm" << "--" << files;
    return job;
}

VcsJob *MercurialPlugin::status(const QList<QUrl> &localLocations, IBasicVersionControl::RecursionMode recursion)
{
    QList<QUrl> locations = localLocations;
    if (recursion == NonRecursive) {
        filterOutDirectories(locations);
    }

    if (locations.empty()) {
        return NULL;
    }

    DVcsJob *job = new DVcsJob(findWorkingDir(locations.first()), this);
    *job << "hg" << "status" << "-A" << "--" << locations;

    connect(job, SIGNAL(readyForParsing(KDevelop::DVcsJob *)), SLOT(parseStatus(KDevelop::DVcsJob *)));

    return job;
}

bool MercurialPlugin::parseStatus(DVcsJob *job) const
{
    if (job->status() != VcsJob::JobSucceeded) {
        mercurialDebug() << "Job failed: " << job->output();
        return false;
    }

    const QString dir = job->directory().absolutePath().append(QDir::separator());
    mercurialDebug() << "Job succeeded for " << dir;
    const QStringList output = job->output().split('\n', QString::SkipEmptyParts);
    QList<QVariant> filestatus;

    QSet<QUrl> conflictedFiles;

    QStringList::const_iterator it = output.constBegin();

    // FIXME: Revive this functionality and add tests for it!
    // conflicts first
//     for (; it != output.constEnd(); it++) {
//         QChar stCh = it->at(0);
//         if (stCh == '%') {
//             it++;
//             break;
//         }
// 
//         QUrl file(it->mid(2).prepend(dir));
// 
//         VcsStatusInfo status;
//         status.setUrl(file);
//         // FIXME: conflicts resolved
//         status.setState(VcsStatusInfo::ItemHasConflicts);
// 
//         conflictedFiles.insert(file);
//         filestatus.append(qVariantFromValue(status));
//     }

    // standard statuses next
    for (; it != output.constEnd(); it++) {
        QChar stCh = it->at(0);

        QUrl file(it->mid(2).prepend(dir));

        if (!conflictedFiles.contains(file)) {
            VcsStatusInfo status;
            status.setUrl(file);
            status.setState(charToState(stCh.toLatin1()));
            filestatus.append(qVariantFromValue(status));
        }
    }

    job->setResults(qVariantFromValue(filestatus));
    return true;
}

VcsJob *MercurialPlugin::revert(const QList<QUrl> &localLocations,
                                IBasicVersionControl::RecursionMode recursion)
{
    QList<QUrl> locations = localLocations;
    if (recursion == NonRecursive) {
        filterOutDirectories(locations);
    }

    if (locations.empty()) {
        return NULL;
    }

    DVcsJob *job = new DVcsJob(findWorkingDir(locations.first()), this);
    *job << "hg" << "revert" << "--" << locations;
    return job;
}

VcsJob *MercurialPlugin::log(const QUrl &localLocation,
                             const VcsRevision &rev,
                             unsigned long limit)
{
    return log(localLocation, VcsRevision::createSpecialRevision(VcsRevision::Start), rev, limit);
}

VcsJob *MercurialPlugin::log(const QUrl &localLocation,
                             const VcsRevision &rev,
                             const VcsRevision &limit)
{
    return log(localLocation, rev, limit, 0);
}


VcsJob *MercurialPlugin::log(const QUrl &localLocation,
                             const VcsRevision &to,
                             const VcsRevision &from,
                             unsigned long limit)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocation), this);

    *job << "hg" << "log" << "-r" << toMercurialRevision(from) + ':' + toMercurialRevision(to);

    if (limit > 0)
        *job << "-l" << QString::number(limit);

    *job << "--template"
         << "{file_copies}\\0{file_dels}\\0{file_adds}\\0{file_mods}\\0{desc}\\0{date|rfc3339date}\\0{author}\\0{parents}\\0{node}\\0{rev}\\0" "--" << localLocation;

    connect(job, SIGNAL(readyForParsing(KDevelop::DVcsJob *)),
            SLOT(parseLogOutputBasicVersionControl(KDevelop::DVcsJob *)));
    return job;
}

VcsJob *MercurialPlugin::annotate(const QUrl &localLocation,
                                  const VcsRevision &rev)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocation), this);

    /*
     * TODO: it would be very nice if we can add "-v" key
     * to get full author name
     */
    *job << "hg" << "annotate" << "-n" << "-u" << "-d";

    QString srev = toMercurialRevision(rev);

    if (srev != QString::null && !srev.isEmpty()) {
        *job << "-r" << srev;
    }

    *job << "--" << localLocation.toLocalFile();

    connect(job, SIGNAL(readyForParsing(KDevelop::DVcsJob *)), SLOT(parseAnnotations(KDevelop::DVcsJob *)));

    return job;
}

KDevelop::VcsJob* MercurialPlugin::mergeBranch(const QUrl &/*repository*/, const QString &/*branchName*/)
{
    // FIXME:
    return nullptr;
}

VcsJob *MercurialPlugin::heads(const QUrl &localLocation)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocation), this);

    // FIXME: Share code
    *job << "hg" << "heads" << "--template"
         << "{file_copies}\\0{file_dels}\\0{file_adds}\\0{file_mods}\\0{desc}\\0{date|rfc3339date}\\0{author}\\0{parents}\\0{node}\\0{rev}\\0" "--" << localLocation;

    connect(job, SIGNAL(readyForParsing(KDevelop::DVcsJob *)),
            SLOT(parseLogOutputBasicVersionControl(KDevelop::DVcsJob *)));
    return job;
}

VcsJob *MercurialPlugin::identify(const QUrl &localLocation)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocation), this);

    *job << "hg" << "identify" << "-n" << "--" << localLocation;

    connect(job, SIGNAL(readyForParsing(KDevelop::DVcsJob *)),
            SLOT(parseIdentify(KDevelop::DVcsJob *)));
    return job;
}

VcsJob *MercurialPlugin::checkoutHead(const QUrl &localLocation, const KDevelop::VcsRevision &rev)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocation), this);

    *job << "hg" << "update" << "-C" << toMercurialRevision(rev);

    return job;
}

VcsJob *MercurialPlugin::mergeWith(const QUrl &localLocation, const KDevelop::VcsRevision &rev)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocation), this);

    *job << "hg" << "merge" << "-r" << toMercurialRevision(rev);

    return job;
}

VcsJob *MercurialPlugin::branch(const QUrl &repository, const VcsRevision &rev, const QString &branchName)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(repository), this);
    *job << "hg" << "branch" << "--" << branchName;
    return job;
}

VcsJob *MercurialPlugin::branches(const QUrl &repository)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(repository), this);
    *job << "hg" << "branches" << "--" << repository;
    connect(job, SIGNAL(readyForParsing(KDevelop::DVcsJob *)), this, SLOT(parseMultiLineOutput(KDevelop::DVcsJob *)));
    return job;
}

VcsJob *MercurialPlugin::currentBranch(const QUrl &repository)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(repository), this);
    *job << "hg" << "branch";
    connect(job, SIGNAL(readyForParsing(KDevelop::DVcsJob *)), this, SLOT(parseMultiLineOutput(KDevelop::DVcsJob *)));
    return job;
}

VcsJob *MercurialPlugin::deleteBranch(const QUrl &repository, const QString &branchName)
{
    return 0;
}

VcsJob *MercurialPlugin::renameBranch(const QUrl &repository, const QString &oldBranchName, const QString &newBranchName)
{
    return 0;
}

VcsJob *MercurialPlugin::switchBranch(const QUrl &repository, const QString &branchName)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(repository), this);
    *job << "hg" << "update" << "--" << branchName;
    return job;
}

VcsJob *MercurialPlugin::tag(const QUrl &repository, const QString &commitMessage, const VcsRevision &rev, const QString &tagName)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(repository), this);
    *job << "hg" << "tag" << "-m" << commitMessage << "-r" << toMercurialRevision(rev) << "--" << tagName;
    return job;
}

DVcsJob *MercurialPlugin::branch(const QString &repository, const QString &basebranch, const QString &branch,
                                 const QStringList &args)
{
    /*
     * Delete branch?
     * TODO: QStringList interface is ugly
     */
    if (args.contains("-D")) {
        /*
         * TODO: ok, nice try, but --close-branch operates only on current branch,
         * so we need to:
         * 1. check there is no uncommited changes
         * 1.1. somehow store them (diff in mem? mq?)
         * 2. switch branch
         * 3. hg commit --close-branch
         * 4. switch back
         */
        return NULL;
#if 0
        DVcsJob *job = new DVcsJob(findWorkingDir(repository), this);
        *job << "hg" << "commit" << "--close-branch" << "-m" << QString("Closing %1 branch").arg(curBranch(repository));
        return job;
#endif
    } else {
        // Only create and delete is supported
        if (args.size() > 0) {
            return NULL;
        }

        // TODO: add support branching from different branch
        if (basebranch != curBranch(repository)) {
            return NULL;
        }

        DVcsJob *job = new DVcsJob(findWorkingDir(repository), this);
        *job << "hg" << "branch" << "--" << branch;
        return job;
    }
}

QString MercurialPlugin::curBranch(const QString &repository)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(repository), this);
    *job << "hg" << "branch";

    QString result;
    if (job->exec() && job->status() == VcsJob::JobSucceeded) {
        // Strip the final newline. Mercurial does not allow whitespaces at beginning or end
        result = job->output().simplified();
    }

    return result;
}

QStringList MercurialPlugin::branches(const QString &repository)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(repository), this);
    *job << "hg" << "branches" << "-q";

    QStringList result;
    if (job->exec() && job->status() == VcsJob::JobSucceeded) {
        result = job->output().split('\n', QString::SkipEmptyParts);
    }

    return result;
}


QList<DVcsEvent> MercurialPlugin::getAllCommits(const QString &repo)
{
    DVcsJob *job = new DVcsJob(repo, this);

    *job << "hg" << "log" << "--template" << "{desc}\\0{date|isodate}\\0{author}\\0{parents}\\0{node}\\0{rev}\\0";

    if (!job->exec() || job->status() != VcsJob::JobSucceeded)
        return QList<DVcsEvent>();

    QList<DVcsEvent> commits;

    parseLogOutput(job, commits);
    return commits;
}

void MercurialPlugin::parseMultiLineOutput(DVcsJob *job) const
{
    job->setResults(job->output().split('\n', QString::SkipEmptyParts));
}

void MercurialPlugin::parseDiff(DVcsJob *job)
{
    if (job->status() != VcsJob::JobSucceeded) {
        mercurialDebug() << "Parse-job failed: " << job->output();
        return;
    }

    // Diffs are generated relativly to the repository root,
    // so we have recover it from the job.
    // Not quite clean m_lastRepoRoot holds the root, after querying isValidDirectory()
    QString workingDir(job->directory().absolutePath());
    isValidDirectory(QUrl(workingDir));
    QString repoRoot = m_lastRepoRoot.path();

    VcsDiff diff;

    // We have to recover the type of the diff somehow.
    diff.setType(VcsDiff::DiffUnified);
    QString output = job->output();

    // hg diff adds a prefix to the paths, which we will now strip
    QRegExp reg("(diff [^\n]+\n--- )a(/[^\n]+\n\\+\\+\\+ )b(/[^\n]+\n)");
    QString replacement("\\1" + repoRoot + "\\2" + repoRoot + "\\3");
    output.replace(reg, replacement);

    diff.setDiff(output);

    job->setResults(qVariantFromValue(diff));
}


bool MercurialPlugin::parseAnnotations(DVcsJob *job) const
{
    if (job->status() != VcsJob::JobSucceeded)
        return false;

    QStringList lines = job->output().split('\n', QString::SkipEmptyParts); // Drops the final empty line, all other are prefixed
    QList<QVariant> result;
    static const QString reAnnotPat("\\s*(\\S+)\\s+(\\d+)\\s+(\\w+ \\w+ \\d\\d \\d\\d:\\d\\d:\\d\\d \\d\\d\\d\\d .\\d\\d\\d\\d): ([^\n]*)");
    QRegExp reAnnot(reAnnotPat, Qt::CaseSensitive, QRegExp::RegExp2);
    unsigned int lineNumber = 0;
    foreach (const QString & line, lines) {
        if (!reAnnot.exactMatch(line)) {
            mercurialDebug() << "Could not parse annotation line: \"" << line << '\"';
            return false;
        }
        VcsAnnotationLine annotation;
        annotation.setLineNumber(lineNumber++);
        annotation.setAuthor(reAnnot.cap(1));
        annotation.setText(reAnnot.cap(4));

        bool success = false;
        qlonglong rev = reAnnot.cap(2).toLongLong(&success);
        if (!success) {
            mercurialDebug() << "Could not parse revision in annotation line: \"" << line << '\"';
            return false;
        }

        QDateTime dt = QDateTime::fromString(reAnnot.cap(3), "%:a %:b %d %H:%M:%S %Y %z");
        mercurialDebug() << reAnnot.cap(3) << dt;
        annotation.setDate(dt);

        VcsRevision vcsrev;
        vcsrev.setRevisionValue(rev, VcsRevision::GlobalNumber);
        annotation.setRevision(vcsrev);
        result.push_back(qVariantFromValue(annotation));
    }

    job->setResults(result);

    return true;
}

void MercurialPlugin::parseLogOutputBasicVersionControl(DVcsJob *job) const
{
    QList<QVariant> events;
    static unsigned int entriesPerCommit = 10;
    QList<QByteArray> items = job->rawOutput().split('\0');

    /* remove trailing \0 */
    items.removeLast();

    if ((items.count() % entriesPerCommit) != 0) {
        mercurialDebug() << "Cannot parse commit log: unexpected number of entries";
        return;
    }

    /* get revision data from items.
     * because there is continuous stream of \0 separated strings,
     * incrementation will occur inside loop body
     *
     * "{desc}\0{date|rfc3339date}\0{author}\0{parents}\0{node}\0{rev}\0"
     * "{file_dels}\0{file_adds}\0{file_mods}\0{file_copies}\0'
     */
    for (QList<QByteArray>::const_iterator it = items.constBegin(); it != items.constEnd();) {
        QString desc = QString::fromLocal8Bit(*it++);
        QString date = QString::fromLocal8Bit(*it++);
        QString author = QString::fromLocal8Bit(*it++);
        QString parents = QString::fromLocal8Bit(*it++);
        QString node = QString::fromLocal8Bit(*it++);
        QString rev = QString::fromLocal8Bit(*it++);

        VcsEvent event;
        event.setMessage(desc);
        event.setDate(QDateTime::fromString(date, Qt::ISODate));
        event.setAuthor(author);

        VcsRevision revision;
        revision.setRevisionValue(rev.toLongLong(), KDevelop::VcsRevision::GlobalNumber);
        event.setRevision(revision);

        QList<VcsItemEvent> items;

        const VcsItemEvent::Action actions[3] = {VcsItemEvent::ContentsModified, VcsItemEvent::Added, VcsItemEvent::Deleted};
        for (int i = 0; i < 3; i++) {
            const QByteArray &files = *it++;
            if (files.isEmpty()) {
                continue;
            }

            foreach (const QByteArray & file, files.split(' ')) {
                VcsItemEvent item;
                item.setActions(actions[i]);
                item.setRepositoryLocation(QUrl::fromPercentEncoding(file));
                items.push_back(item);
            }
        }

        const QByteArray &copies = *it++;
        if (!copies.isEmpty()) {
            foreach (const QByteArray & copy, copies.split(' ')) {
                QList<QByteArray> files = copy.split('~');

                VcsItemEvent item;
                item.setActions(VcsItemEvent::Copied);
                item.setRepositoryCopySourceLocation(QUrl::fromPercentEncoding(files[0]));
                item.setRepositoryLocation(QUrl::fromPercentEncoding(files[1]));
                items.push_back(item);
            }
        }


        event.setItems(items);
        events.push_front(QVariant::fromValue(event));
    }

    job->setResults(QVariant(events));
}

void MercurialPlugin::parseIdentify(DVcsJob *job) const
{
    QString value = job->output();
    QList<QVariant> result;

    // remove last '\n'
    value.chop(1);

    // remove last '+' if necessary
    if (value.endsWith('+'))
        value.chop(1);

    foreach (const QString & rev, value.split('+')) {
        VcsRevision revision;
        revision.setRevisionValue(rev.toLongLong(), VcsRevision::GlobalNumber);
        result << qVariantFromValue<VcsRevision>(revision);
    }
    job->setResults(QVariant(result));
}


void MercurialPlugin::parseLogOutput(const DVcsJob *job, QList<DVcsEvent> &commits) const
{
    mercurialDebug() << "parseLogOutput";

    static unsigned int entriesPerCommit = 6;
    QList<QByteArray> items = job->rawOutput().split('\0');

    if (uint(items.size()) < entriesPerCommit || 1 != (items.size() % entriesPerCommit)) {
        mercurialDebug() << "Cannot parse commit log: unexpected number of entries";
        return;
    }

    bool success = false;

    QString const &lastRev = items.at(entriesPerCommit - 1);
    unsigned int id = lastRev.toUInt(&success);

    if (!success) {
        mercurialDebug() << "Could not parse last revision \"" << lastRev << '"';
        id = 1024;
    }

    QVector<QString> fullIds(id + 1);

    typedef std::reverse_iterator<QList<QByteArray>::const_iterator> QStringListReverseIterator;
    QStringListReverseIterator rbegin(items.constEnd() - 1), rend(items.constBegin());  // Skip the final 0
    unsigned int lastId;

    while (rbegin != rend) {
        QString rev = QString::fromLocal8Bit(*rbegin++);
        QString node = QString::fromLocal8Bit(*rbegin++);
        QString parents = QString::fromLocal8Bit(*rbegin++);
        QString author = QString::fromLocal8Bit(*rbegin++);
        QString date = QString::fromLocal8Bit(*rbegin++);
        QString desc = QString::fromLocal8Bit(*rbegin++);
        lastId = id;
        id = rev.toUInt(&success);

        if (!success) {
            mercurialDebug() << "Could not parse revision \"" << rev << '"';
            return;
        }

        if (uint(fullIds.size()) <= id) {
            fullIds.resize(id * 2);
        }

        fullIds[id] = node;

        DVcsEvent commit;
        commit.setCommit(node);
        commit.setAuthor(author);
        commit.setDate(date);
        commit.setLog(desc);

        if (id == 0) {
            commit.setType(DVcsEvent::INITIAL);
        } else {
            if (parents.isEmpty() && id != 0) {
                commit.setParents(QStringList(fullIds[lastId]));
            } else {
                QStringList parentList;
                QStringList unparsedParentList = parents.split(QChar(' '), QString::SkipEmptyParts);
                // id:Short-node
                static const unsigned int shortNodeSuffixLen = 13;
                foreach (const QString & p, unparsedParentList) {
                    QString ids = p.left(p.size() - shortNodeSuffixLen);
                    id = ids.toUInt(&success);

                    if (!success) {
                        mercurialDebug() << "Could not parse parent-revision \"" << ids << "\" of revision " << rev;
                        return;
                    }

                    parentList.push_back(fullIds[id]);
                }

                commit.setParents(parentList);
            }
        }

        commits.push_front(commit);
    }
}

VcsLocationWidget *MercurialPlugin::vcsLocation(QWidget *parent) const
{
    return new MercurialVcsLocationWidget(parent);
}

void MercurialPlugin::filterOutDirectories(QList<QUrl> &locations)
{
    QList<QUrl> fileLocations;
    foreach (const QUrl & location, locations) {
        if (!QFileInfo(location.toLocalFile()).isDir()) {
            fileLocations << location;
        }
    }
    locations = fileLocations;
}

VcsStatusInfo::State MercurialPlugin::charToState(const char ch)
{
    switch (ch) {
    case 'M':
        return VcsStatusInfo::ItemModified;
    case 'A':
        return VcsStatusInfo::ItemAdded;
    case 'R':
        return VcsStatusInfo::ItemDeleted;
    case 'C':
        return VcsStatusInfo::ItemUpToDate;
    case '!':   // Missing
        return VcsStatusInfo::ItemUserState;
    default:
        return VcsStatusInfo::ItemUnknown;
    }
}

QString MercurialPlugin::toMercurialRevision(const VcsRevision &vcsrev)
{
    switch (vcsrev.revisionType()) {
    case VcsRevision::Special:
        switch (vcsrev.revisionValue().value<KDevelop::VcsRevision::RevisionSpecialType>()) {
        case VcsRevision::Head:
            return QString("tip");
        case VcsRevision::Base:
            return QString(".");
        case VcsRevision::Working:
            return QString("");
        case VcsRevision::Previous: // We can't determine this from one revision alone
            return QString::null;
        case VcsRevision::Start:
            return QString("0");
        default:
            return QString();
        }
    case VcsRevision::GlobalNumber:
        return QString::number(vcsrev.revisionValue().toLongLong());
    case VcsRevision::Date:         // TODO
    case VcsRevision::FileNumber:   // No file number for mercurial
    default:
        return QString::null;
    }
}

QDir MercurialPlugin::findWorkingDir(const QUrl &location)
{
    QFileInfo fileInfo(location.toLocalFile());

    // find out correct working directory
    if (fileInfo.isFile()) {
        return fileInfo.absolutePath();
    } else {
        return fileInfo.absoluteFilePath();
    }
}

QUrl MercurialPlugin::remotePushRepositoryLocation(QDir &directory)
{
    // check default-push first
    DVcsJob *job = new DVcsJob(directory, this);
    *job << "hg" << "paths" << "default-push";
    if (!job->exec() || job->status() != VcsJob::JobSucceeded) {
        mercurialDebug() << "no default-push, hold on";

        // or try default
        job = new DVcsJob(directory, this);
        *job << "hg" << "paths" << "default";

        if (!job->exec() || job->status() != VcsJob::JobSucceeded) {
            mercurialDebug() << "nowhere to push!";

            // fail is everywhere
            return QUrl();
        }
    }

    // don't forget to strip last '\n'
    return job->output().trimmed();
}

void MercurialPlugin::registerRepositoryForCurrentBranchChanges(const QUrl &repository)
{
    // TODO
}

void MercurialPlugin::additionalMenuEntries(QMenu *menu, const QList<QUrl> &urls)
{
    m_urls = urls;

    // FIXME:
//     menu->addAction(m_headsAction);
//     menu->addSeparator()->setText(i18n("Mercurial Queues"));
//     menu->addAction(m_mqNew);
//     menu->addAction(m_mqPushAction);
//     menu->addAction(m_mqPushAllAction);
//     menu->addAction(m_mqPopAction);
//     menu->addAction(m_mqPopAllAction);
//     menu->addAction(m_mqManagerAction);
// 
//     m_headsAction->setEnabled(m_urls.count() == 1);
// 
//     //FIXME:not supported yet, so disable
//     m_mqNew->setEnabled(false);
//     m_mqPushAction->setEnabled(false);
//     m_mqPushAllAction->setEnabled(false);
//     m_mqPopAction->setEnabled(false);
//     m_mqPopAllAction->setEnabled(false);
}

void MercurialPlugin::showHeads()
{
    const QUrl &current = m_urls.first();
    MercurialHeadsWidget *headsWidget = new MercurialHeadsWidget(this, current);
    headsWidget->show();
}

void MercurialPlugin::showMercurialQueuesManager()
{
    const QUrl &current = m_urls.first();
    MercurialQueuesManager *managerWidget = new MercurialQueuesManager(this, current);
    managerWidget->show();
}

/*
 * Mercurial Queues
 */

VcsJob *MercurialPlugin::mqNew(const QUrl &localLocation, const QString &name, const QString &message)
{
    return 0;
}

VcsJob *MercurialPlugin::mqPush(const QUrl &localLocation)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocation), this);
    *job << "hg" << "qpush";
    return job;
}

VcsJob *MercurialPlugin::mqPushAll(const QUrl &localLocation)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocation), this);
    *job << "hg" << "qpush" << "-a";
    return job;
}

VcsJob *MercurialPlugin::mqPop(const QUrl &localLocation)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocation), this);
    *job << "hg" << "qpop";
    return job;
}

VcsJob *MercurialPlugin::mqPopAll(const QUrl &localLocation)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocation), this);
    *job << "hg" << "qpop" << "-a";
    return job;
}

VcsJob *MercurialPlugin::mqApplied(const QUrl &localLocation)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocation), this);
    *job << "hg" << "qapplied";
    connect(job, SIGNAL(readyForParsing(KDevelop::DVcsJob *)), this, SLOT(parseMultiLineOutput(KDevelop::DVcsJob *)));
    return job;
}

VcsJob *MercurialPlugin::mqUnapplied(const QUrl &localLocation)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocation), this);
    *job << "hg" << "qunapplied";
    connect(job, SIGNAL(readyForParsing(KDevelop::DVcsJob *)), this, SLOT(parseMultiLineOutput(KDevelop::DVcsJob *)));
    return job;
}

// #include "mercurialplugin.moc"
