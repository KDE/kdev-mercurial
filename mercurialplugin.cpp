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

#include <KDE/KPluginFactory>
#include <KDE/KPluginLoader>
#include <KDE/KLocalizedString>
#include <KDE/KAboutData>
#include <KDE/KDateTime>
#include <KDE/KDebug>

#include <interfaces/icore.h>

#include <vcs/vcsjob.h>
#include <vcs/vcsevent.h>
#include <vcs/vcsrevision.h>
#include <vcs/vcsannotation.h>
#include <vcs/dvcs/dvcsjob.h>
#include <interfaces/icore.h>


K_PLUGIN_FACTORY(KDevMercurialFactory, registerPlugin<MercurialPlugin>();)
K_EXPORT_PLUGIN(KDevMercurialFactory(KAboutData("kdevmercurial", "kdevmercurial", ki18n("Mercurial"), "0.1", ki18n("A plugin to support Mercurial version control systems"), KAboutData::License_GPL)))

using namespace KDevelop;

MercurialPlugin::MercurialPlugin(QObject *parent, const QVariantList &)
        : DistributedVersionControlPlugin(parent, KDevMercurialFactory::componentData())
{
    KDEV_USE_EXTENSION_INTERFACE(KDevelop::IBasicVersionControl)
    KDEV_USE_EXTENSION_INTERFACE(KDevelop::IDistributedVersionControl)

    core()->uiController()->addToolView(i18n("Mercurial"), dvcsViewFactory());
    setXMLFile("kdevmercurial.rc");
}

MercurialPlugin::~MercurialPlugin()
{
}


void MercurialPlugin::unload()
{
    core()->uiController()->removeToolView( dvcsViewFactory() );
}


QString MercurialPlugin::name() const
{
    return QLatin1String("Mercurial");
}

bool MercurialPlugin::isValidDirectory(const KUrl & directory)
{
    // Mercurial uses the same test, so we don't lose any functionality
    static const QString hgDir(".hg");

    if (m_lastRepoRoot.isParentOf(directory))
        return true;

    const QString initialPath(directory.toLocalFile(KUrl::RemoveTrailingSlash));
    const QFileInfo finfo(initialPath);
    QDir dir;
    if (finfo.isFile()) {
        dir = finfo.absoluteDir();
    } else {
        dir = QDir(initialPath);
        dir.makeAbsolute();
    }

    while (!dir.cd(hgDir) && dir.cdUp()) {} // cdUp, until there is a sub-directory called .hg

    if (hgDir != dir.dirName())
        return false;

    dir.cdUp(); // Leave .hg
    m_lastRepoRoot.setDirectory(dir.absolutePath());
    return true;
}

bool MercurialPlugin::isVersionControlled(const KUrl & url)
{
    const QFileInfo fsObject(url.toLocalFile());

    if (!fsObject.isFile()) {
        return isValidDirectory(url);
    }

    // Clean, Added, Modified. Escape possible files starting with "-"
    static const QStringList versionControlledFlags(QString("-c -a -m --").split(' '));
    const QString absolutePath = fsObject.absolutePath();
    QStringList listFile(versionControlledFlags);
    listFile.push_back(fsObject.fileName());
    const QStringList filesInDir = getLsFiles(absolutePath, listFile);

    return !filesInDir.empty();
}

VcsJob* MercurialPlugin::init(const KUrl &directory)
{
    DVcsJob *job = new DVcsJob(directory.path(), this);

    *job << "hg" << "init";

    return job;
}

VcsJob* MercurialPlugin::repositoryLocation(const KUrl & directory)
{
    return NULL;
}

VcsJob* MercurialPlugin::createWorkingCopy(const VcsLocation & localOrRepoLocationSrc, const KUrl& destinationDirectory, IBasicVersionControl::RecursionMode)
{
    DVcsJob *job = new DVcsJob(destinationDirectory.path(), this);

    *job << "hg" << "clone" << "--" << localOrRepoLocationSrc.localUrl().pathOrUrl() << destinationDirectory.pathOrUrl();

    return job;
}

VcsJob* MercurialPlugin::pull(const VcsLocation & otherRepository, const KUrl& workingRepository)
{
    DVcsJob *job = new DVcsJob(workingRepository.toLocalFile(), this);

    *job << "hg" << "pull" << "--";

    QString pathOrUrl = otherRepository.localUrl().pathOrUrl();

    if (!pathOrUrl.isEmpty())
        *job << pathOrUrl;

    return job;
}

VcsJob* MercurialPlugin::push(const KUrl &workingRepository, const VcsLocation & otherRepository)
{
    DVcsJob *job = new DVcsJob(workingRepository.toLocalFile(), this);

    *job << "hg" << "push" << "--";

    QString pathOrUrl = otherRepository.localUrl().pathOrUrl();

    if (!pathOrUrl.isEmpty())
        *job << pathOrUrl;

    return job;
}

VcsJob* MercurialPlugin::add(const KUrl::List& localLocations, IBasicVersionControl::RecursionMode recursion)
{
    KUrl::List locations = localLocations;

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

VcsJob* MercurialPlugin::copy(const KUrl& localLocationSrc, const KUrl& localLocationDst)
{
    kDebug() << "copy";
    return NULL;
#if 0
    std::auto_ptr<DVcsJob> job(new DVcsJob(this));

    if (!prepareJob(job.get(), localLocationSrc.toLocalFile())) {
        return NULL;
    }

    *job << "hg" << "cp" << "--" << localLocationSrc.toLocalFile() << localLocationDst.path();

    return job.release();
#endif
}

VcsJob* MercurialPlugin::move(const KUrl& localLocationSrc,
                const KUrl& localLocationDst)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocationSrc), this);
    *job << "hg" << "mv" << "--" << localLocationSrc.toLocalFile() << localLocationDst.path();
    return job;
}

//If no files specified then commit already added files
VcsJob* MercurialPlugin::commit(const QString& message,
                                  const KUrl::List& localLocations,
                                  IBasicVersionControl::RecursionMode recursion)
{
    KUrl::List locations = localLocations;

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

VcsJob* MercurialPlugin::update(const KUrl::List& files,
                                const KDevelop::VcsRevision& rev,
                                KDevelop::IBasicVersionControl::RecursionMode recursion)
{
    return NULL;

#if 0
    if (files.empty())
        return NULL;

    std::auto_ptr<DVcsJob> job(new DVcsJob(this));

    if (!prepareJob(job.get(), files.front().toLocalFile())) {
        return NULL;
    }

    //Note: the message is quoted somewhere else, so if we quote here then we have quotes in the commit log
    *job << "hg" << "revert" << "-r" << toMercurialRevision(rev) << "--";

    if (!addDirsConditionally(job.get(), files, recursion)) {
        return NULL;
    }

    return job.release();
#endif
}

VcsJob* MercurialPlugin::resolve(const KUrl::List& files, KDevelop::IBasicVersionControl::RecursionMode recursion)
{
    return NULL;

#if 0
    if (files.empty())
        return NULL;

    std::auto_ptr<DVcsJob> job(new DVcsJob(this));

    if (!prepareJob(job.get(), files.front().toLocalFile())) {
        return NULL;
    }

    //Note: the message is quoted somewhere else, so if we quote here then we have quotes in the commit log
    *job << "hg" << "resolve" << "--";

    if (!addDirsConditionally(job.get(), files, recursion)) {
        return NULL;
    }

    return job.release();
#endif
}

VcsJob* MercurialPlugin::diff(const KUrl& fileOrDirectory,
                const VcsRevision & srcRevision,
                const VcsRevision & dstRevision,
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

    kDebug() << "Diff between" << srcRevision.prettyValue() << '(' << srcRev << ')'<< "and" << dstRevision.prettyValue() << '(' << dstRev << ')'<< " requested";

    if ((srcRev.isNull() && dstRev.isNull())
        || (srcRev.isEmpty() && dstRev.isEmpty())
        ) {
        kError() << "Diff between" << srcRevision.prettyValue() << '(' << srcRev << ')'<< "and" << dstRevision.prettyValue() << '(' << dstRev << ')'<< " not possible";
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
    } else if (srcRev.isNull()) {  // Changes made in one arbitrary revision
        *job << "-c" << dstRev;
    } else {
        *job << "-r" << srcRev;
        if (!dstRev.isEmpty()) {
            *job << "-r" << dstRev;
        }
    }

    *job << "--" << srcPath;

    connect(job, SIGNAL(readyForParsing(KDevelop::DVcsJob*)), SLOT(parseDiff(KDevelop::DVcsJob*)));

    return job;
}

VcsJob* MercurialPlugin::remove(const KUrl::List& files)
{
    if (files.empty()) {
        return NULL;
    }

    DVcsJob *job = new DVcsJob(findWorkingDir(files.first()), this);
    *job << "hg" << "rm" << "--" << files;
    return job;
}

VcsJob* MercurialPlugin::status(const KUrl::List& localLocations, IBasicVersionControl::RecursionMode recursion)
{
    KUrl::List locations = localLocations;
    if (recursion == NonRecursive) {
        filterOutDirectories(locations);
    }

    if (locations.empty()) {
        return NULL;
    }

    DVcsJob *job = new DVcsJob(findWorkingDir(locations.first()), this);
    *job << "hg" << "status" << "-A" << "--" << locations;

    connect(job, SIGNAL(readyForParsing(KDevelop::DVcsJob*)), SLOT(parseStatus(KDevelop::DVcsJob*)));

    return job;
}

bool MercurialPlugin::parseStatus(DVcsJob *job) const
{
    if (job->status() != VcsJob::JobSucceeded) {
        kDebug() << "Job failed: " << job->output();
        return false;
    }

    const QString dir = job->directory().absolutePath().append(QDir::separator());
    kDebug() << "Job succeeded for " << dir;
    const QStringList output = job->output().split('\n', QString::SkipEmptyParts);
    QList<QVariant> filestatus;
    foreach(const QString &line, output) {
        QChar stCh = line.at(0);

        KUrl file(line.mid(2).prepend(dir));

        VcsStatusInfo status;
        status.setUrl(file);
        status.setState(charToState(stCh.toAscii()));

        filestatus.append(qVariantFromValue(status));
    }

    job->setResults(qVariantFromValue(filestatus));
    return true;
}

VcsJob* MercurialPlugin::revert(const KUrl::List& localLocations,
                                   IBasicVersionControl::RecursionMode recursion)
{
    KUrl::List locations = localLocations;
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

VcsJob* MercurialPlugin::log(const KUrl& localLocation,
                const VcsRevision& rev,
                unsigned long limit)
{
    return log(localLocation, VcsRevision::createSpecialRevision(VcsRevision::Start), rev, limit);
}

VcsJob* MercurialPlugin::log(const KUrl& localLocation,
                const VcsRevision& rev,
                const VcsRevision& limit)
{
    return log(localLocation, rev, limit, 0);
}


VcsJob* MercurialPlugin::log(const KUrl& localLocation,
                const VcsRevision& to,
                const VcsRevision& from,
                unsigned long limit)
{
    DVcsJob *job = new DVcsJob(findWorkingDir(localLocation), this);

    *job << "hg" << "log" << "-r" << toMercurialRevision(from) + ':' + toMercurialRevision(to);

    if (limit > 0)
        *job << "-l" << QString::number(limit);

    *job << "--template"
         << "{file_copies}\\0{file_dels}\\0{file_adds}\\0{file_mods}\\0{desc}\\0{date|rfc3339date}\\0{author}\\0{parents}\\0{node}\\0{rev}\\0"
         << "--" << localLocation;

    connect(job, SIGNAL(readyForParsing(KDevelop::DVcsJob*)),
            SLOT(parseLogOutputBasicVersionControl(KDevelop::DVcsJob*)));
    return job;
}

VcsJob* MercurialPlugin::annotate(const KUrl& localLocation,
                            const VcsRevision& rev)
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

    connect(job, SIGNAL(readyForParsing(KDevelop::DVcsJob*)), SLOT(parseAnnotations(KDevelop::DVcsJob*)));

    return job;
}


DVcsJob* MercurialPlugin::switchBranch(const QString &repository, const QString &branch)
{
    return NULL;

#if 0
    std::auto_ptr<DVcsJob> job(new DVcsJob(this));

    if (!prepareJob(job.get(), repository)) {
        return NULL;
    }

    *job << "hg" << "update" << "--" << branch;

    return job.release();
#endif
}

DVcsJob* MercurialPlugin::branch(const QString &repository, const QString &basebranch, const QString &branch,
                                   const QStringList &args)
{
    return NULL;

#if 0
    Q_UNUSED(repository)
    Q_UNUSED(basebranch)
    Q_UNUSED(branch)

    if (args.size() > 0) // Mercurial doesn't support rename or delete operations, which are hidden in the args of the function call
        return NULL;

    std::auto_ptr<DVcsJob> job(new DVcsJob(this));

    if (!prepareJob(job.get(), repository)) {
        return NULL;
    }

    if (basebranch != curBranch(repository)) { // I'm to lazy to support branching from different branch (which isn't used the GUI)
        return NULL;
    }

    *job << "hg" << "branch" << "--" << branch;

    return job.release();
#endif
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
    return QList<DVcsEvent>();

#if 0
    std::auto_ptr<DVcsJob> job(new DVcsJob(this));
    job->setAutoDelete(false);

    if (!prepareJob(job.get(), repo)) {
        return QList<DVcsEvent>();
    }

    *job << "hg" << "log" << "--template" << "{desc}\\0{date|isodate}\\0{author}\\0{parents}\\0{node}\\0{rev}\\0";

    if (!job->exec() || job->status() != VcsJob::JobSucceeded)
        return QList<DVcsEvent>();

    QList<DVcsEvent> commits;

    parseLogOutput(job.get(), commits);

    return commits;
#endif
}

void MercurialPlugin::parseDiff(DVcsJob* job)
{
    if (job->status() != VcsJob::JobSucceeded) {
        kDebug() << "Parse-job failed: " << job->output();
        return;
    }

    // Diffs are generated relativly to the repository root,
    // so we have recover it from the job.
	// Not quite clean m_lastRepoRoot holds the root, after querying isValidDirectory()
    QString workingDir(job->directory().absolutePath());
    isValidDirectory(KUrl(workingDir));
    QString repoRoot = m_lastRepoRoot.path(KUrl::RemoveTrailingSlash);

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
    foreach(const QString & line, lines) {
        if (!reAnnot.exactMatch(line)) {
            kDebug() << "Could not parse annotation line: \"" << line << '\"';
            return false;
        }
        VcsAnnotationLine annotation;
        annotation.setLineNumber(lineNumber++);
        annotation.setAuthor(reAnnot.cap(1));
        annotation.setText(reAnnot.cap(4));

        bool success = false;
        qlonglong rev = reAnnot.cap(2).toLongLong(&success);
        if (!success) {
            kDebug() << "Could not parse revision in annotation line: \"" << line << '\"';
            return false;
        }

        KDateTime dt = KDateTime::fromString(reAnnot.cap(3), "%:a %:b %d %H:%M:%S %Y %z");
        kDebug() << reAnnot.cap(3) << dt;
        annotation.setDate(dt.dateTime());

        VcsRevision vcsrev;
        vcsrev.setRevisionValue(rev, VcsRevision::GlobalNumber);
        annotation.setRevision(vcsrev);
        result.push_back(qVariantFromValue(annotation));
    }

    job->setResults(result);

    return true;
}

void MercurialPlugin::parseLogOutputBasicVersionControl(DVcsJob* job) const
{
    QList<QVariant> events;
    static unsigned int entriesPerCommit = 10;
    QList<QByteArray> items = job->rawOutput().split('\0');

    /* remove trailing \0 */
    items.removeLast();

    if ((items.count() % entriesPerCommit) != 0) {
        kDebug() << "Cannot parse commit log: unexpected number of entries";
        return;
    }

    /* get revision data from items.
     * because there is continuous stream of \0 separated strings,
     * incrementation will occur inside loop body
     *
     * "{file_copies}\\0{file_dels}\\0{file_adds}\\0{file_mods}\\0"
     * "{desc}\\0{date|rfc3339date}\\0{author}\\0{parents}\\0{node}\\0{rev}\\0"
     */
    for(QList<QByteArray>::const_iterator it = items.begin(); it != items.end(); ) {
        QString copies = QString::fromLocal8Bit(*it++);
        QString dels = QString::fromLocal8Bit(*it++);
        QString adds = QString::fromLocal8Bit(*it++);
        QString mods = QString::fromLocal8Bit(*it++);
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
        revision.setRevisionValue(rev, KDevelop::VcsRevision::GlobalNumber);
        event.setRevision(revision);

        QList<VcsItemEvent> items;

        // TODO: Convince Mercurial to separate the files with newlines, in order to allow whitespaces in filenames
        foreach (const QString & file, mods.split(QChar(' '), QString::SkipEmptyParts)) {
            VcsItemEvent item;
            item.setActions(VcsItemEvent::ContentsModified);
            item.setRevision(revision);
            item.setRepositoryLocation(file);
            items.push_back(item);
        }

        foreach (const QString & file, adds.split(QChar(' '), QString::SkipEmptyParts)) {
            VcsItemEvent item;
            item.setActions(VcsItemEvent::Added);
            item.setRevision(revision);
            item.setRepositoryLocation(file);
            items.push_back(item);
        }

        foreach (const QString & file, dels.split(QChar(' '), QString::SkipEmptyParts)) {
            VcsItemEvent item;
            item.setActions(VcsItemEvent::Deleted);
            item.setRevision(revision);
            item.setRepositoryLocation(file);
            items.push_back(item);
        }

        //TODO: copied files

        event.setItems(items);
        events.push_back(QVariant::fromValue(event));
    }

    job->setResults(QVariant(events));
}

void MercurialPlugin::parseLogOutput(const DVcsJob * job, QList<DVcsEvent>& commits) const
{
    kDebug() << "parseLogOutput";

#if 0
    static unsigned int entriesPerCommit = 6;
    QList<QByteArray> items = job->rawOutput().split('\0');

    if (uint(items.size()) < entriesPerCommit || 1 != (items.size() % entriesPerCommit)) {
        kDebug() << "Cannot parse commit log: unexpected number of entries";
        return;
    }

    bool success = false;

    QString const & lastRev = items.at(entriesPerCommit - 1);
    unsigned int id = lastRev.toUInt(&success);

    if (!success) {
        kDebug() << "Could not parse last revision \"" << lastRev << '"';
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
            kDebug() << "Could not parse revision \"" << rev << '"';
            return;
        }

        if (uint(fullIds.size()) <= id) {
            fullIds.resize(id*2);
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
                foreach(const QString & p, unparsedParentList) {
                    QString ids = p.left(p.size() - shortNodeSuffixLen);
                    id = ids.toUInt(&success);

                    if (!success) {
                        kDebug() << "Could not parse parent-revision \"" << ids << "\" of revision " << rev;
                        return;
                    }

                    parentList.push_back(fullIds[id]);
                }

                commit.setParents(parentList);
            }
        }

        commits.push_front(commit);
    }
#endif
}

VcsLocationWidget* MercurialPlugin::vcsLocation(QWidget* parent) const
{
    return NULL;
}

QStringList MercurialPlugin::getLsFiles(const QString &directory, const QStringList &args)
{
    DVcsJob *job = new DVcsJob(directory, this);
    *job << "hg" << "status" << "-n";

    if (!args.isEmpty()) {
        *job << args;
    }

    if (!job->exec() || job->status() != VcsJob::JobSucceeded) {
        delete job;
        return QStringList();
    }

    const QString prefix = directory.endsWith(QDir::separator()) ? directory : directory + QDir::separator();
    QStringList fileList = job->output().split('\n', QString::SkipEmptyParts);
    for (QStringList::iterator it = fileList.begin(); it != fileList.end(); ++it) {
        it->prepend(prefix);
    }

    delete job;
    return fileList;
}

void MercurialPlugin::filterOutDirectories(KUrl::List &locations)
{
    KUrl::List fileLocations;
    foreach(const KUrl &location, locations) {
        if(!QFileInfo(location.toLocalFile()).isDir()) {
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
        //ToDo: hasConflicts
    default:
        return VcsStatusInfo::ItemUnknown;
    }
}

QString MercurialPlugin::toMercurialRevision(const VcsRevision & vcsrev)
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
    case VcsRevision::Date:			// TODO
    case VcsRevision::FileNumber:   // No file number for mercurial
    default:
        return QString::null;
    }
}

QDir MercurialPlugin::findWorkingDir(const KUrl& location)
{
    QFileInfo fileInfo(location.toLocalFile());

    // find out correct working directory
    if (fileInfo.isFile()) {
        return fileInfo.absolutePath();
    } else {
        return fileInfo.absoluteFilePath();
    }
}

// #include "mercurialplugin.moc"
