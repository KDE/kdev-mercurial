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

#include <QDir>
#include <QDateTime>
#include <QFileInfo>
#include <QAction>
#include <QMenu>
#include <QDateTime>
#include <QStandardPaths>
#include <QTimer>

#include <KDirWatch>

#include <interfaces/icore.h>

#include <vcs/vcsjob.h>
#include <vcs/vcsevent.h>
#include <vcs/vcsrevision.h>
#include <vcs/vcsannotation.h>
#include <vcs/vcslocation.h>
#include <vcs/dvcs/dvcsjob.h>

#include <util/path.h>

#include "mercurialjob.h"
#include "mercurialvcslocationwidget.h"
#include "mercurialannotatejob.h"
#include "mercurialpushjob.h"
#include "ui/mercurialheadswidget.h"
#include "ui/mercurialqueuesmanager.h"

#include "debug.h"

using namespace KDevelop;

namespace
{
    QString logTemplate()
    {
        return QStringLiteral("{desc}\\_%{date|rfc3339date}\\_%{author}\\_%{parents}\\_%{node}\\_%{rev}\\_%{file_dels}\\_%{file_adds}\\_%{file_mods}\\_%{file_copies}\\_%");
    }
}

MercurialPlugin::MercurialPlugin(QObject *parent, const QVariantList &)
    : DistributedVersionControlPlugin(parent, QStringLiteral("kdevmercurial"))
{
    if (QStandardPaths::findExecutable(QStringLiteral("hg")).isEmpty()) {
        setErrorDescription(i18n("Unable to find hg executable. Is it installed on the system?"));
        return;
    }

    m_headsAction = new QAction(i18n("Heads..."), this);
    m_mqNew = new QAction(i18nc("mercurial queues submenu", "New..."), this);
    m_mqPushAction = new QAction(i18nc("mercurial queues submenu", "Push"), this);
    m_mqPushAllAction = new QAction(i18nc("mercurial queues submenu", "Push All"), this);
    m_mqPopAction = new QAction(i18nc("mercurial queues submenu", "Pop"), this);
    m_mqPopAllAction = new QAction(i18nc("mercurial queues submenu", "Pop All"), this);
    m_mqManagerAction = new QAction(i18nc("mercurial queues submenu", "Manager..."), this);

    connect(m_headsAction, &QAction::triggered, this, &MercurialPlugin::showHeads);
    connect(m_mqManagerAction, &QAction::triggered, this, &MercurialPlugin::showMercurialQueuesManager);

    m_watcher = new KDirWatch(this);
    connect(m_watcher, &KDirWatch::dirty, this, &MercurialPlugin::fileChanged);
    connect(m_watcher, &KDirWatch::created, this, &MercurialPlugin::fileChanged);
}

MercurialPlugin::~MercurialPlugin()
{
}

QString MercurialPlugin::name() const
{
    return QLatin1String("Mercurial");
}

void MercurialPlugin::fileChanged(const QString& file)
{
    Q_ASSERT(file.endsWith(QStringLiteral("branch")));
    const QUrl repoUrl = Path(file).parent().parent().toUrl();

    m_branchesChange.append(repoUrl);
    QTimer::singleShot(1000, this, [this](){emit repositoryBranchChanged(m_branchesChange.takeFirst());});
}

bool MercurialPlugin::isValidDirectory(const QUrl &directory)
{
    // Mercurial uses the same test, so we don't lose any functionality
    static const QString hgDir(".hg");

    if (m_lastRepoRoot.isParentOf(directory))
        return true;

    const QString initialPath(directory.adjusted(QUrl::StripTrailingSlash).toLocalFile());
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

bool MercurialPlugin::isValidRemoteRepositoryUrl(const QUrl& remoteLocation)
{
    Q_UNUSED(remoteLocation);
    // TODO
    return false;
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

VcsJob *MercurialPlugin::repositoryLocation(const QUrl &/*directory*/)
{
    return nullptr;
}

VcsJob *MercurialPlugin::createWorkingCopy(const VcsLocation &localOrRepoLocationSrc, const QUrl &destinationDirectory, IBasicVersionControl::RecursionMode)
{
    DVcsJob *job = new DVcsJob(QDir::home(), this, KDevelop::OutputJob::Silent);

    *job << "hg" << "clone" << "--" << localOrRepoLocationSrc.localUrl().toLocalFile() << destinationDirectory.toLocalFile();

    return job;
}

VcsJob *MercurialPlugin::pull(const VcsLocation &otherRepository, const QUrl &workingRepository)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(workingRepository), this);

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

    if (locations.empty()) {
        // nothing left after filtering
        return nullptr;
    }

    DVcsJob *job = new DVcsJob(findWorkingDir(locations.first()), this);
    *job << "hg" << "add" << "--" << locations;
    return job;
}

VcsJob *MercurialPlugin::copy(const QUrl &localLocationSrc, const QUrl &localLocationDst)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocationSrc), this, KDevelop::OutputJob::Silent);
    *job << "hg" << "cp" << "--" << localLocationSrc.toLocalFile() << localLocationDst.path();
    return job;
}

VcsJob *MercurialPlugin::move(const QUrl &localLocationSrc,
                              const QUrl &localLocationDst)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocationSrc), this, KDevelop::OutputJob::Silent);
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
        return nullptr;
    }

    DVcsJob *job = new DVcsJob(findWorkingDir(locations.first()), this, KDevelop::OutputJob::Silent);
    *job << "hg" << "commit" << "-m" << message << "--" << locations;
    return job;
}

VcsJob *MercurialPlugin::update(const QList<QUrl> &localLocations,
                                const VcsRevision &rev,
                                IBasicVersionControl::RecursionMode recursion)
{
    if (rev.revisionType() == VcsRevision::Special &&
            rev.revisionValue().value<VcsRevision::RevisionSpecialType>() == VcsRevision::Head) {
        return pull(VcsLocation(), QUrl::fromLocalFile(findWorkingDir(localLocations.first()).path()));
    }

    QList<QUrl> locations = localLocations;

    if (recursion == NonRecursive) {
        filterOutDirectories(locations);
    }

    if (locations.empty()) {
        return nullptr;
    }

    DVcsJob *job = new DVcsJob(findWorkingDir(locations.first()), this);


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
        return nullptr;
    }

    DVcsJob *job = new DVcsJob(findWorkingDir(fileList.first()), this);

    //Note: the message is quoted somewhere else, so if we quote here then we have quotes in the commit log
    *job << "hg" << "resolve" << "--" << fileList;

    return job;
}

VcsJob *MercurialPlugin::diff(const QUrl &fileOrDirectory,
                              const VcsRevision &srcRevision,
                              const VcsRevision &dstRevision,
                              IBasicVersionControl::RecursionMode recursionMode)
{
    if (!fileOrDirectory.isLocalFile()) {
        return nullptr;
    }

    // non-standard searching for working directory
    QString workingDir;
    QFileInfo fileInfo(fileOrDirectory.toLocalFile());

    // It's impossible to non-recursively diff directories
    if (recursionMode == NonRecursive && fileInfo.isDir()) {
        return nullptr;
    }

    // find out correct working directory
    if (fileInfo.isFile()) {
        workingDir = fileInfo.absolutePath();
    } else {
        workingDir = fileInfo.absoluteFilePath();
    }

    QString srcRev = toMercurialRevision(srcRevision);
    QString dstRev = toMercurialRevision(dstRevision);

    qCDebug(PLUGIN_MERCURIAL) << "Diff between" << srcRevision.prettyValue() << '(' << srcRev << ')' << "and" << dstRevision.prettyValue() << '(' << dstRev << ')' << " requested";

    if (
        (srcRev.isNull() && dstRev.isNull()) ||
        (srcRev.isEmpty() && dstRev.isEmpty())
    ) {
        qWarning() << "Diff between" << srcRevision.prettyValue() << '(' << srcRev << ')' << "and" << dstRevision.prettyValue() << '(' << dstRev << ')' << " not possible";
        return nullptr;
    }

    const QString srcPath = fileOrDirectory.toLocalFile();

    DVcsJob *job = new DVcsJob(workingDir, this, KDevelop::OutputJob::Silent);

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

    // unified diff
    *job << "-U" << "3";    // Default from GNU diff

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

    connect(job, &DVcsJob::readyForParsing, this, &MercurialPlugin::parseDiff);

    return job;
}

VcsJob *MercurialPlugin::remove(const QList<QUrl> &files)
{
    if (files.empty()) {
        return nullptr;
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
        return nullptr;
    }

    DVcsJob *job = new DVcsJob(findWorkingDir(locations.first()), this, KDevelop::OutputJob::Silent);
    *job << "hg" << "status" << "-A" << "--" << locations;

    connect(job, &DVcsJob::readyForParsing, this, &MercurialPlugin::parseStatus);

    return job;
}

bool MercurialPlugin::parseStatus(DVcsJob *job) const
{
    if (job->status() != VcsJob::JobSucceeded) {
        qCDebug(PLUGIN_MERCURIAL) << "Job failed: " << job->output();
        return false;
    }

    const QString dir = job->directory().absolutePath().append(QDir::separator());
    qCDebug(PLUGIN_MERCURIAL) << "Job succeeded for " << dir;
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
//         QUrl file = QUrl::fromLocalFile(it->mid(2).prepend(dir));
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

        QUrl file = QUrl::fromLocalFile(it->mid(2).prepend(dir));

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
        return nullptr;
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
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocation), this, KDevelop::OutputJob::Silent);

    *job << "hg" << "log" << "-r" << toMercurialRevision(from) + ':' + toMercurialRevision(to);

    if (limit > 0)
        *job << "-l" << QString::number(limit);

    *job << "--template"
         << logTemplate() << "--" << localLocation;

    connect(job, &DVcsJob::readyForParsing, this,
            &MercurialPlugin::parseLogOutputBasicVersionControl);
    return job;
}

VcsJob *MercurialPlugin::annotate(const QUrl &localLocation,
                                  const VcsRevision &rev)
{
    return new MercurialAnnotateJob(findWorkingDir(localLocation), rev, localLocation, this);
}

class MergeBranchJob : public MercurialJob
{
public:
	MergeBranchJob(const QDir &workingDir, const QString& branchName, MercurialPlugin* parent)
	: MercurialJob(workingDir, parent, JobType::Merge),
	m_branchName(branchName)
	{}

	void start() override
	{
		m_status = JobRunning;

		auto job = new DVcsJob(m_workingDir, vcsPlugin(), KDevelop::OutputJob::Silent);
		*job << "hg" << "log" << "-b" << m_branchName << "-l" << "1" << "--template" << "{rev}";

		connect(job, &DVcsJob::resultsReady, this, [this](VcsJob* j) {
			auto job = static_cast<DVcsJob *>(j);
			if (job->status() != VcsJob::JobSucceeded || job->output().isEmpty())
				return setFail();

			auto mergeJob = new DVcsJob(m_workingDir, vcsPlugin());

			*mergeJob << "hg" << "merge" << "-r" << job->output().trimmed();

			connect(mergeJob, &KJob::finished, this, [this](KJob *job) {
				if (job->error()) {
					setFail();
				}
			});
			connect(mergeJob, &DVcsJob::resultsReady, this, [this](VcsJob* j) {
				auto job = static_cast<DVcsJob *>(j);
				if (job->status() != VcsJob::JobSucceeded || job->output().isEmpty())
					return setFail();
				setSuccess();
			});

			m_job = mergeJob;
			mergeJob->start();
		});
		connect(job, &KJob::finished, this, [this](KJob *job) {
			if (job->error()) {
				setFail();
			}
		});
		m_job = job;
		job->start();
	}

private:
	QString m_branchName;
};

KDevelop::VcsJob* MercurialPlugin::mergeBranch(const QUrl& repository, const QString& branchName)
{
    Q_ASSERT(!branchName.isEmpty());

    return new MergeBranchJob(findWorkingDir(repository), branchName, this);
}

VcsJob *MercurialPlugin::heads(const QUrl &localLocation)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocation), this, KDevelop::OutputJob::Silent);

    *job << "hg" << "heads" << "--template" << logTemplate();

    connect(job, &DVcsJob::readyForParsing, this,
            &MercurialPlugin::parseLogOutputBasicVersionControl);
    return job;
}

VcsJob *MercurialPlugin::identify(const QUrl &localLocation)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocation), this, KDevelop::OutputJob::Silent);

    *job << "hg" << "identify" << "-n";

    connect(job, &DVcsJob::readyForParsing, this,
            &MercurialPlugin::parseIdentify);
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

VcsJob *MercurialPlugin::branch(const QUrl &repository, const VcsRevision &/*rev*/, const QString &branchName)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(repository), this, OutputJob::Silent);
    *job << "hg" << "branch" << "--" << branchName;
    return job;
}

VcsJob *MercurialPlugin::branches(const QUrl &repository)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(repository), this, OutputJob::Silent);
    *job << "hg" << "branches";
    connect(job, &DVcsJob::readyForParsing, this, &MercurialPlugin::parseMultiLineOutput);
    return job;
}

VcsJob *MercurialPlugin::currentBranch(const QUrl &repository)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(repository), this, OutputJob::Silent);
    *job << "hg" << "branch";
    connect(job, &DVcsJob::readyForParsing, this, &MercurialPlugin::parseMultiLineOutput);
    return job;
}

VcsJob *MercurialPlugin::deleteBranch(const QUrl &/*repository*/, const QString &/*branchName*/)
{
    // Not supported
    return nullptr;
}

VcsJob *MercurialPlugin::renameBranch(const QUrl &/*repository*/, const QString &/*oldBranchName*/, const QString &/*newBranchName*/)
{
    // Not supported
    return nullptr;
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

QVector<DVcsEvent> MercurialPlugin::allCommits(const QString& repo)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(QUrl::fromLocalFile(repo)), this, OutputJob::Silent);

    *job << "hg" << "log" << "--template" << "{desc}\\_%{date|isodate}\\_%{author}\\_%{parents}\\_%{node}\\_%{rev}\\_%";

    if (!job->exec() || job->status() != VcsJob::JobSucceeded)
        return QVector<DVcsEvent>();

    QVector<DVcsEvent> commits;

    parseLogOutput(job, commits);
    return commits;
}

void MercurialPlugin::parseMultiLineOutput(DVcsJob *job) const
{
    qCDebug(PLUGIN_MERCURIAL) << job->output();
    job->setResults(job->output().split('\n', QString::SkipEmptyParts));
}

void MercurialPlugin::parseDiff(DVcsJob *job)
{
    if (job->status() != VcsJob::JobSucceeded) {
        qCDebug(PLUGIN_MERCURIAL) << "Parse-job failed: " << job->output();
        return;
    }

    // Diffs are generated relativly to the repository root,
    // so we have recover it from the job.
    // Not quite clean m_lastRepoRoot holds the root, after querying isValidDirectory()
    QString workingDir(job->directory().absolutePath());
    isValidDirectory(QUrl::fromLocalFile(workingDir));
    QString repoRoot = m_lastRepoRoot.adjusted(QUrl::StripTrailingSlash).path();

    VcsDiff diff;

    // We have to recover the type of the diff somehow.
    QString output = job->output();

    // hg diff adds a prefix to the paths, which we will now strip
    QRegExp reg("(diff [^\n]+\n--- )a(/[^\n]+\n\\+\\+\\+ )b(/[^\n]+\n)");
    QString replacement("\\1" + repoRoot + "\\2" + repoRoot + "\\3");
    output.replace(reg, replacement);

    diff.setDiff(output);
    diff.setBaseDiff(QUrl::fromLocalFile(repoRoot));
    diff.setDepth(0);

    job->setResults(qVariantFromValue(diff));
}

void MercurialPlugin::parseLogOutputBasicVersionControl(DVcsJob *job) const
{
    QList<QVariant> events;
    static unsigned int entriesPerCommit = 10;
    auto items = job->output().split("\\_%");

    qCDebug(PLUGIN_MERCURIAL) << items;
    /* remove trailing \\_% */
    items.removeLast();

    if ((items.count() % entriesPerCommit) != 0) {
        qCDebug(PLUGIN_MERCURIAL) << "Cannot parse commit log: unexpected number of entries";
        return;
    }

    /* get revision data from items.
     * because there is continuous stream of \\_% separated strings,
     * incrementation will occur inside loop body
     *
     * "{desc}\\_%{date|rfc3339date}\\_%{author}\\_%{parents}\\_%{node}\\_%{rev}\\_%"
     * "{file_dels}\\_%{file_adds}\\_%{file_mods}\\_%{file_copies}\\_%'
     */
    for (auto it = items.constBegin(); it != items.constEnd();) {
        QString desc = *it++;
        qCDebug(PLUGIN_MERCURIAL) << desc;
        Q_ASSERT(!desc.isEmpty());
        QString date = *it++;
        qCDebug(PLUGIN_MERCURIAL) << date;
        Q_ASSERT(!date.isEmpty());
        QString author = *it++;
        qCDebug(PLUGIN_MERCURIAL) << author;
        Q_ASSERT(!author.isEmpty());
        QString parents = *it++;
        QString node = *it++;
        QString rev = *it++;
        qCDebug(PLUGIN_MERCURIAL) << rev;
        Q_ASSERT(!rev.isEmpty());

        VcsEvent event;
        event.setMessage(desc);
        event.setDate(QDateTime::fromString(date, Qt::ISODate));
        event.setAuthor(author);

        VcsRevision revision;
        revision.setRevisionValue(rev.toLongLong(), KDevelop::VcsRevision::GlobalNumber);
        event.setRevision(revision);

        QList<VcsItemEvent> items;

        const VcsItemEvent::Action actions[3] = {VcsItemEvent::Deleted, VcsItemEvent::Added, VcsItemEvent::ContentsModified};
        for (int i = 0; i < 3; i++) {
            const auto &files = *it++;
            if (files.isEmpty()) {
                continue;
            }

            foreach (const auto& file, files.split(' ')) {
                VcsItemEvent item;
                item.setActions(actions[i]);
                item.setRepositoryLocation(QUrl::fromPercentEncoding(file.toLocal8Bit()));
                items.push_back(item);
            }
        }

        const auto &copies = *it++;
        if (!copies.isEmpty()) {
            foreach (const auto & copy, copies.split(' ')) {
                auto files = copy.split('~');

                // TODO: What is it? Why it doesn't work
                if (files.size() >= 2) {
                    VcsItemEvent item;
                    item.setActions(VcsItemEvent::Copied);
                    item.setRepositoryCopySourceLocation(QUrl::fromPercentEncoding(files[0].toLocal8Bit()));
                    item.setRepositoryLocation(QUrl::fromPercentEncoding(files[1].toLocal8Bit()));
                    items.push_back(item);
                }
            }
        }

        event.setItems(items);
        events.push_back(QVariant::fromValue(event));
    }

    job->setResults(QVariant(events));
}

void MercurialPlugin::parseIdentify(DVcsJob *job) const
{
    QString value = job->output();
    QList<QVariant> result;

    qCDebug(PLUGIN_MERCURIAL) << value;
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


void MercurialPlugin::parseLogOutput(const DVcsJob* job, QVector<DVcsEvent>& commits) const
{
    qCDebug(PLUGIN_MERCURIAL) << "parseLogOutput";

    static unsigned int entriesPerCommit = 6;
    auto items = job->output().split("\\_%");

    if (uint(items.size()) < entriesPerCommit || 1 != (items.size() % entriesPerCommit)) {
        qCDebug(PLUGIN_MERCURIAL) << "Cannot parse commit log: unexpected number of entries";
        return;
    }

    bool success = false;

    QString const &lastRev = items.at(entriesPerCommit - 1);
    unsigned int id = lastRev.toUInt(&success);

    if (!success) {
        qCDebug(PLUGIN_MERCURIAL) << "Could not parse last revision \"" << lastRev << '"';
        id = 1024;
    }

    QVector<QString> fullIds(id + 1);

    typedef std::reverse_iterator<QStringList::const_iterator> QStringListReverseIterator;
    QStringListReverseIterator rbegin(items.constEnd() - 1), rend(items.constBegin());  // Skip the final 0
    unsigned int lastId;

    while (rbegin != rend) {
        QString rev = *rbegin++;
        QString node = *rbegin++;
        QString parents = *rbegin++;
        QString author = *rbegin++;
        QString date = *rbegin++;
        QString desc = *rbegin++;
        lastId = id;
        id = rev.toUInt(&success);

        if (!success) {
            qCDebug(PLUGIN_MERCURIAL) << "Could not parse revision \"" << rev << '"';
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
                        qCDebug(PLUGIN_MERCURIAL) << "Could not parse parent-revision \"" << ids << "\" of revision " << rev;
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
            return QString();
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
        return QString();
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
        qCDebug(PLUGIN_MERCURIAL) << "no default-push, hold on";

        // or try default
        job = new DVcsJob(directory, this);
        *job << "hg" << "paths" << "default";

        if (!job->exec() || job->status() != VcsJob::JobSucceeded) {
            qCDebug(PLUGIN_MERCURIAL) << "nowhere to push!";

            // fail is everywhere
            return QUrl();
        }
    }

    // don't forget to strip last '\n'
    return QUrl::fromLocalFile(job->output().trimmed());
}

void MercurialPlugin::registerRepositoryForCurrentBranchChanges(const QUrl& repository)
{
    if (isValidDirectory(repository)) {
        QString headFile = m_lastRepoRoot.path() + QStringLiteral("/.hg/branch");
        m_watcher->addFile(headFile);
    }
}

void MercurialPlugin::additionalMenuEntries(QMenu *menu, const QList<QUrl> &urls)
{
    m_urls = urls;

    menu->addAction(m_headsAction);
    menu->addSeparator()->setText(i18n("Mercurial Queues"));
    menu->addAction(m_mqNew);
    menu->addAction(m_mqPushAction);
    menu->addAction(m_mqPushAllAction);
    menu->addAction(m_mqPopAction);
    menu->addAction(m_mqPopAllAction);
    menu->addAction(m_mqManagerAction);

    m_headsAction->setEnabled(m_urls.count() == 1);

    //FIXME:not supported yet, so disable
    m_mqNew->setEnabled(false);
    m_mqPushAction->setEnabled(false);
    m_mqPushAllAction->setEnabled(false);
    m_mqPopAction->setEnabled(false);
    m_mqPopAllAction->setEnabled(false);
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

VcsJob *MercurialPlugin::mqNew(const QUrl &/*localLocation*/, const QString &/*name*/, const QString &/*message*/)
{
    return nullptr;
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
    connect(job, &DVcsJob::readyForParsing, this, &MercurialPlugin::parseMultiLineOutput);
    return job;
}

VcsJob *MercurialPlugin::mqUnapplied(const QUrl &localLocation)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocation), this);
    *job << "hg" << "qunapplied";
    connect(job, &DVcsJob::readyForParsing, this, &MercurialPlugin::parseMultiLineOutput);
    return job;
}

// #include "mercurialplugin.moc"
