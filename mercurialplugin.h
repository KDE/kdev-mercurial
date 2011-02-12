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

#ifndef MERCURIAL_PLUGIN_H
#define MERCURIAL_PLUGIN_H

#include <vcs/interfaces/idistributedversioncontrol.h>
#include <vcs/dvcs/dvcsplugin.h>
#include <QtCore/QObject>
#include <vcs/vcsstatusinfo.h>

namespace KDevelop
{

class VcsJob;

class VcsRevision;
}

class MercurialExecutor;

/**
 * This is the main class of KDevelop's Mercurial plugin.
 *
 * It implements the DVCS dependent things not implemented in KDevelop::DistributedVersionControlPlugin
 * @author Fabian Wiesel <fabian.wiesel@googlemail.com>
 */

class MercurialPlugin
            : public KDevelop::DistributedVersionControlPlugin
{
    Q_OBJECT
    Q_INTERFACES(KDevelop::IBasicVersionControl KDevelop::IDistributedVersionControl)

public:
    explicit MercurialPlugin(QObject *parent, const QVariantList & args = QVariantList());
    ~MercurialPlugin();

    virtual void unload();

    bool isValidDirectory(const KUrl &dirPath);
    bool isVersionControlled(const KUrl &path);
    QString name() const;

    KDevelop::VcsJob* init(const KUrl & directory);
    KDevelop::VcsJob* repositoryLocation(const KUrl & directory);   // Not implemented.
    KDevelop::VcsJob* createWorkingCopy(const KDevelop::VcsLocation & localOrRepoLocationSrc, const KUrl & repository, IBasicVersionControl::RecursionMode = KDevelop::IBasicVersionControl::Recursive);
    KDevelop::VcsJob* add(const KUrl::List& localLocations,
                 KDevelop::IBasicVersionControl::RecursionMode recursion  = KDevelop::IBasicVersionControl::Recursive);
    KDevelop::VcsJob* copy(const KUrl& localLocationSrc,
                  const KUrl& localLocationDst);
    KDevelop::VcsJob* move(const KUrl& localLocationSrc,
                  const KUrl& localLocationDst);

    KDevelop::VcsJob* commit(const QString& message,
                    const KUrl::List& localLocations,
                    KDevelop::IBasicVersionControl::RecursionMode recursion);
    KDevelop::VcsJob* diff(const KUrl& fileOrDirectory,
                  const KDevelop::VcsRevision & srcRevision,
                  const KDevelop::VcsRevision & dstRevision,
                  KDevelop::VcsDiff::Type diffType,
                  KDevelop::IBasicVersionControl::RecursionMode recursionMode);

    KDevelop::VcsJob* remove(const KUrl::List& files);
    KDevelop::VcsJob* status(const KUrl::List& localLocations,
                    KDevelop::IBasicVersionControl::RecursionMode recursion);
    KDevelop::VcsJob* revert(const KUrl::List& localLocations,
                    KDevelop::IBasicVersionControl::RecursionMode recursion);
    KDevelop::VcsJob* update(const KUrl::List& files,
                    const KDevelop::VcsRevision& rev, KDevelop::IBasicVersionControl::RecursionMode recursion);
    KDevelop::VcsJob* resolve(const KUrl::List& files, KDevelop::IBasicVersionControl::RecursionMode recursion);

    KDevelop::VcsJob* log(const KUrl& localLocation,
                const KDevelop::VcsRevision& rev,
                unsigned long limit);
    KDevelop::VcsJob* log(const KUrl& localLocation,
                const KDevelop::VcsRevision& rev,
                const KDevelop::VcsRevision& limit);

    KDevelop::VcsJob* log(const KUrl& localLocation,
                const KDevelop::VcsRevision& to,
                const KDevelop::VcsRevision& from,
                unsigned long limit);
    KDevelop::VcsJob* annotate(const KUrl& localLocation,
                            const KDevelop::VcsRevision& rev);

    KDevelop::VcsJob* reset(const KUrl& repository, const QStringList &args, const KUrl::List &files);

    KDevelop::DVcsJob* switchBranch(const QString &repository, const QString &branch);
    KDevelop::DVcsJob* branch(const QString &repository, const QString &basebranch = QString(), const QString &branch = QString(),
                    const QStringList &args = QStringList());

    KDevelop::VcsJob* push(const KUrl& localRepositoryLocation,
                          const KDevelop::VcsLocation& localOrRepoLocationDst);
    KDevelop::VcsJob* pull(const KDevelop::VcsLocation& localOrRepoLocationSrc,
                          const KUrl& localRepositoryLocation);

    KDevelop::VcsLocationWidget* vcsLocation(QWidget* parent) const;
public:
    //parsers for branch:
    QString curBranch(const QString &repository);
    QStringList branches(const QString &repository);

    //graph helpers
    QList<DVcsEvent> getAllCommits(const QString &repo);

protected slots:
    void parseLogOutputBasicVersionControl(KDevelop::DVcsJob *job) const;
    bool parseStatus(KDevelop::DVcsJob *job) const;
    bool parseAnnotations(KDevelop::DVcsJob *job) const;
    void parseDiff(KDevelop::DVcsJob *job);

protected:
    //used in log
    void parseLogOutput(const KDevelop::DVcsJob *job, QList<DVcsEvent>& commits) const;

    static QString toMercurialRevision(const KDevelop::VcsRevision & vcsrev);
    static bool addDirsConditionally(KDevelop::DVcsJob* job, const KUrl::List & locations, KDevelop::IBasicVersionControl::RecursionMode recursion);
    static KDevelop::VcsStatusInfo::State charToState(const char ch);

    QStringList getLsFiles(const QString &directory, const QStringList &args = QStringList());
    KUrl m_lastRepoRoot;
};

#endif
