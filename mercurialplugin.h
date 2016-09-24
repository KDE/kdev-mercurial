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
#include <QtCore/QDir>
#include <vcs/vcsstatusinfo.h>

class QAction;
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
    explicit MercurialPlugin(QObject *parent, const QVariantList &args = QVariantList());
    ~MercurialPlugin();

    bool isValidDirectory(const QUrl &dirPath);
    bool isVersionControlled(const QUrl &path);
    QString name() const;

    KDevelop::VcsJob *init(const QUrl &directory);
    KDevelop::VcsJob *repositoryLocation(const QUrl &directory);    // Not implemented.
    KDevelop::VcsJob *createWorkingCopy(const KDevelop::VcsLocation &localOrRepoLocationSrc, const QUrl &repository, IBasicVersionControl::RecursionMode = KDevelop::IBasicVersionControl::Recursive);
    KDevelop::VcsJob *add(const QList<QUrl> &localLocations,
                          KDevelop::IBasicVersionControl::RecursionMode recursion  = KDevelop::IBasicVersionControl::Recursive);
    KDevelop::VcsJob *copy(const QUrl &localLocationSrc,
                           const QUrl &localLocationDst);
    KDevelop::VcsJob *move(const QUrl &localLocationSrc,
                           const QUrl &localLocationDst);

    KDevelop::VcsJob *commit(const QString &message,
                             const QList<QUrl> &localLocations,
                             KDevelop::IBasicVersionControl::RecursionMode recursion);
    KDevelop::VcsJob *diff(const QUrl &fileOrDirectory,
                           const KDevelop::VcsRevision &srcRevision,
                           const KDevelop::VcsRevision &dstRevision,
                           KDevelop::VcsDiff::Type diffType,
                           KDevelop::IBasicVersionControl::RecursionMode recursionMode);

    KDevelop::VcsJob *remove(const QList<QUrl> &files);
    KDevelop::VcsJob *status(const QList<QUrl> &localLocations,
                             KDevelop::IBasicVersionControl::RecursionMode recursion);
    KDevelop::VcsJob *revert(const QList<QUrl> &localLocations,
                             KDevelop::IBasicVersionControl::RecursionMode recursion);
    KDevelop::VcsJob *update(const QList<QUrl> &localLocations,
                             const KDevelop::VcsRevision &rev, KDevelop::IBasicVersionControl::RecursionMode recursion);
    KDevelop::VcsJob *resolve(const QList<QUrl> &files, KDevelop::IBasicVersionControl::RecursionMode recursion);

    KDevelop::VcsJob *log(const QUrl &localLocation,
                          const KDevelop::VcsRevision &rev,
                          unsigned long limit);
    KDevelop::VcsJob *log(const QUrl &localLocation,
                          const KDevelop::VcsRevision &rev,
                          const KDevelop::VcsRevision &limit);

    KDevelop::VcsJob *log(const QUrl &localLocation,
                          const KDevelop::VcsRevision &to,
                          const KDevelop::VcsRevision &from,
                          unsigned long limit);
    KDevelop::VcsJob *annotate(const QUrl &localLocation,
                               const KDevelop::VcsRevision &rev);

    KDevelop::VcsJob* mergeBranch(const QUrl &repository, const QString &branchName);

    // mercurial specific stuff
    KDevelop::VcsJob *heads(const QUrl &localLocation);
    KDevelop::VcsJob *identify(const QUrl &localLocation);
    KDevelop::VcsJob *checkoutHead(const QUrl &localLocation, const KDevelop::VcsRevision &rev);
    KDevelop::VcsJob *mergeWith(const QUrl &localLocation, const KDevelop::VcsRevision &rev);

    // mercurial queues stuff
    KDevelop::VcsJob *mqNew(const QUrl &localLocation, const QString &name, const QString &message);
    KDevelop::VcsJob *mqPush(const QUrl &localLocation);
    KDevelop::VcsJob *mqPushAll(const QUrl &localLocation);
    KDevelop::VcsJob *mqPop(const QUrl &localLocation);
    KDevelop::VcsJob *mqPopAll(const QUrl &localLocation);
    KDevelop::VcsJob *mqApplied(const QUrl &localLocation);
    KDevelop::VcsJob *mqUnapplied(const QUrl &localLocation);

    KDevelop::DVcsJob *switchBranch(const QString &repository, const QString &branch);
    KDevelop::DVcsJob *branch(const QString &repository, const QString &basebranch = QString(), const QString &branch = QString(), const QStringList &args = QStringList());

    KDevelop::VcsJob *push(const QUrl &localRepositoryLocation,
                           const KDevelop::VcsLocation &localOrRepoLocationDst);
    KDevelop::VcsJob *pull(const KDevelop::VcsLocation &localOrRepoLocationSrc,
                           const QUrl &localRepositoryLocation);

    KDevelop::VcsLocationWidget *vcsLocation(QWidget *parent) const;

    //parsers for branch:
    KDevelop::VcsJob *branch(const QUrl &repository, const KDevelop::VcsRevision &rev, const QString &branchName);
    KDevelop::VcsJob *branches(const QUrl &repository);
    KDevelop::VcsJob *currentBranch(const QUrl &repository);
    KDevelop::VcsJob *deleteBranch(const QUrl &repository, const QString &branchName);
    KDevelop::VcsJob *renameBranch(const QUrl &repository, const QString &oldBranchName, const QString &newBranchName);
    KDevelop::VcsJob *switchBranch(const QUrl &repository, const QString &branchName);
    KDevelop::VcsJob *tag(const QUrl &repository, const QString &commitMessage, const KDevelop::VcsRevision &rev, const QString &tagName);

    QString curBranch(const QString &repository);
    QStringList branches(const QString &repository);

    //graph helpers
    QList<DVcsEvent> getAllCommits(const QString &repo);

    /**
     * Find out where is default remote located.
     * @param directory inside working directory
     */
    QUrl remotePushRepositoryLocation(QDir &directory);

    void registerRepositoryForCurrentBranchChanges(const QUrl &repository);

protected slots:
    void parseLogOutputBasicVersionControl(KDevelop::DVcsJob *job) const;
    bool parseStatus(KDevelop::DVcsJob *job) const;
    bool parseAnnotations(KDevelop::DVcsJob *job) const;
    void parseDiff(KDevelop::DVcsJob *job);
    void parseMultiLineOutput(KDevelop::DVcsJob *job) const;

    /*
     * mercurial specific stuff
     */
    void parseIdentify(KDevelop::DVcsJob *job) const;

    /*
     * ui helpers
     */
    void showHeads();
    void showMercurialQueuesManager();

protected:
    //used in log
    void parseLogOutput(const KDevelop::DVcsJob *job, QList<DVcsEvent> &commits) const;

    static QString toMercurialRevision(const KDevelop::VcsRevision &vcsrev);
    /**
     * Remove directories from @p locations.
     */
    static void filterOutDirectories(QList<QUrl> &locations);

    static KDevelop::VcsStatusInfo::State charToState(const char ch);
    static QDir findWorkingDir(const QUrl &location);

    QUrl m_lastRepoRoot;

    // actions
    QAction *m_headsAction;

    QAction *m_mqNew,
            *m_mqPushAction,
            *m_mqPushAllAction,
            *m_mqPopAction,
            *m_mqPopAllAction,
            *m_mqManagerAction;
    QList<QUrl> m_urls;
    void additionalMenuEntries(QMenu *menu, const QList<QUrl> &urls);
};

//class MercurialQueues

#endif
