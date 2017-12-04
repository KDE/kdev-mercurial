/*
    This file is part of KDevelop

    Copyright 2008 Evgeniy Ivanov <powerfox@kde.ru>
    Copyright 2009 Fabian Wiesel <fabian.wiesel@fu-berlin.de>
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

#include "test_mercurial.h"

#include <QTest>
#include <QSignalSpy>
#include <tests/testcore.h>
#include <tests/autotestshell.h>

#include <QScopedPointer>
#include <QUrl>

#include <KIO/DeleteJob>

#include <vcs/dvcs/dvcsjob.h>
#include <vcs/vcsannotation.h>
#include <vcs/vcslocation.h>

#include "../mercurialplugin.h"
#include "../mercurialannotatejob.h"
#include "../models/mercurialheadsmodel.h"
#include "debug.h"

using namespace KDevelop;

namespace
{
const QString tempDir = QDir::tempPath();
const QString MercurialTestDir1("kdevMercurial_testdir");
const QString mercurialTest_BaseDir(tempDir + "/kdevMercurial_testdir/");
const QString mercurialTest_BaseDir2(tempDir + "/kdevMercurial_testdir2/");
const QString mercurialRepo(mercurialTest_BaseDir + ".hg");
const QString mercurialSrcDir(mercurialTest_BaseDir + "src/");
const QString mercurialTest_FileName("testfile");
const QString mercurialTest_FileName2("foo");
const QString mercurialTest_FileName3("bar");

void verifyJobSucceed(VcsJob* job)
{
    QVERIFY(job);
    QVERIFY(job->exec());
    QVERIFY(job->status() == KDevelop::VcsJob::JobSucceeded);
}

void writeToFile(const QString& path, const QString& content, QIODevice::OpenModeFlag mode = QIODevice::WriteOnly)
{
    QFile f(path);
    QVERIFY(f.open(mode));
    QTextStream input(&f);
    input << content << endl;
}

}

void MercurialTest::initTestCase()
{
    AutoTestShell::init({"kdevmercurial"});
    m_testCore = new KDevelop::TestCore();
    m_testCore->initialize(KDevelop::Core::NoUi);

    m_proxy = new MercurialPlugin(m_testCore);
}

void MercurialTest::cleanupTestCase()
{
    delete m_proxy;

    removeTempDirs();
}

void MercurialTest::init()
{
    QDir tmpdir(tempDir);
    tmpdir.mkdir(mercurialTest_BaseDir);
    tmpdir.mkdir(mercurialSrcDir);
    tmpdir.mkdir(mercurialTest_BaseDir2);
}

void MercurialTest::cleanup()
{
    removeTempDirs();
}

void MercurialTest::repoInit()
{
    qCDebug(PLUGIN_MERCURIAL) << "Trying to init repo";
    // create the local repository
    VcsJob *j = m_proxy->init(QUrl::fromLocalFile(mercurialTest_BaseDir));
    verifyJobSucceed(j);
    QVERIFY(QFileInfo(mercurialRepo).exists());

    QVERIFY(m_proxy->isValidDirectory(QUrl::fromLocalFile(mercurialTest_BaseDir)));
    QVERIFY(!m_proxy->isValidDirectory(QUrl::fromLocalFile("/tmp")));
}

void MercurialTest::addFiles()
{
    qCDebug(PLUGIN_MERCURIAL) << "Adding files to the repo";

    writeToFile(mercurialTest_BaseDir + mercurialTest_FileName, "commit 0 content");
    writeToFile(mercurialTest_BaseDir + mercurialTest_FileName2, "commit 0 content, foo");

    VcsJob *j = m_proxy->status({QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);
    auto statusResults = j->fetchResults().toList();
    QCOMPARE(statusResults.size(), 2);
    auto status = statusResults[0].value<VcsStatusInfo>();
    QCOMPARE(status.url(), QUrl::fromLocalFile(mercurialTest_BaseDir + mercurialTest_FileName2));
    QCOMPARE(status.state(), VcsStatusInfo::ItemUnknown);
    status = statusResults[1].value<VcsStatusInfo>();
    QCOMPARE(status.url(), QUrl::fromLocalFile(mercurialTest_BaseDir + mercurialTest_FileName));
    QCOMPARE(status.state(), VcsStatusInfo::ItemUnknown);

    j = m_proxy->add({QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::NonRecursive);
    QVERIFY(!j);

    // /tmp/kdevMercurial_testdir/ and kdevMercurial_testdir
    //add always should use aboslute path to the any directory of the repository, let's check:
    j = m_proxy->add({QUrl::fromLocalFile(mercurialTest_BaseDir), QUrl::fromLocalFile(MercurialTestDir1)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);

    // /tmp/kdevMercurial_testdir/ and testfile
    j = m_proxy->add({QUrl::fromLocalFile(mercurialTest_BaseDir + mercurialTest_FileName)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);

    writeToFile(mercurialSrcDir + mercurialTest_FileName3, "commit 0 content, bar");

    j = m_proxy->status({QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);
    statusResults = j->fetchResults().toList();
    QCOMPARE(statusResults.size(), 3);
    status = statusResults[0].value<VcsStatusInfo>();
    QCOMPARE(status.url(), QUrl::fromLocalFile(mercurialTest_BaseDir + mercurialTest_FileName2));
    QCOMPARE(status.state(), VcsStatusInfo::ItemAdded);
    status = statusResults[1].value<VcsStatusInfo>();
    QCOMPARE(status.url(), QUrl::fromLocalFile(mercurialTest_BaseDir + mercurialTest_FileName));
    QCOMPARE(status.state(), VcsStatusInfo::ItemAdded);
    status = statusResults[2].value<VcsStatusInfo>();
    QCOMPARE(status.url(), QUrl::fromLocalFile(mercurialSrcDir + mercurialTest_FileName3));
    QCOMPARE(status.state(), VcsStatusInfo::ItemUnknown);

    // repository path without trailing slash and a file in a parent directory
    // /tmp/repo  and /tmp/repo/src/bar
    j = m_proxy->add({QUrl::fromLocalFile(mercurialSrcDir + mercurialTest_FileName3)});
    verifyJobSucceed(j);

    // let's use absolute path, because it's used in ContextMenus
    j = m_proxy->add({QUrl::fromLocalFile(mercurialTest_BaseDir + mercurialTest_FileName2)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);

    //Now let's create several files and try "hg add file1 file2 file3"
    writeToFile(mercurialTest_BaseDir + "file1", "file1");
    writeToFile(mercurialTest_BaseDir + "file2", "file2");

    j = m_proxy->add({QUrl::fromLocalFile(mercurialTest_BaseDir + "file1"), QUrl::fromLocalFile(mercurialTest_BaseDir + "file2")}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);
}

void MercurialTest::commitFiles()
{
    qCDebug(PLUGIN_MERCURIAL) << "Committing...";

    VcsJob *j = m_proxy->commit(QString("commit 0"), {QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);

    j = m_proxy->status({QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);

    // Test the results of the "mercurial add"
    DVcsJob *jobLs = new DVcsJob(mercurialTest_BaseDir, nullptr);
    *jobLs << "hg" << "stat" << "-q" << "-c" << "-n";
    verifyJobSucceed(jobLs);

    QStringList files = jobLs->output().split("\n");
    QVERIFY(files.contains(mercurialTest_FileName));
    QVERIFY(files.contains(mercurialTest_FileName2));
    QVERIFY(files.contains("src/" + mercurialTest_FileName3));

    qCDebug(PLUGIN_MERCURIAL) << "Committing one more time";

    // let's try to change the file and test "hg commit -a"
    writeToFile(mercurialTest_BaseDir + mercurialTest_FileName, "commit 1 content", QIODevice::Append);

    j = m_proxy->add({QUrl::fromLocalFile(mercurialTest_BaseDir + mercurialTest_FileName)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);

    j = m_proxy->commit(QString("commit 1"), {QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);
}

void MercurialTest::cloneRepository()
{
    // make job that clones the local repository, created in the previous test
    VcsJob *j = m_proxy->createWorkingCopy(VcsLocation(mercurialTest_BaseDir), QUrl::fromLocalFile(mercurialTest_BaseDir2));
    verifyJobSucceed(j);
    QVERIFY(QFileInfo(QString(mercurialTest_BaseDir2 + "/.hg/")).exists());
}

void MercurialTest::testInit()
{
    repoInit();
}

void MercurialTest::testAdd()
{
    repoInit();
    addFiles();
}

void MercurialTest::testCommit()
{
    repoInit();
    addFiles();
    commitFiles();
}

void MercurialTest::testBranching()
{
    repoInit();
    addFiles();
    commitFiles();

    auto j = m_proxy->branches(QUrl::fromLocalFile(mercurialTest_BaseDir));
    verifyJobSucceed(j);
    auto branches = j->fetchResults().toList();
    QCOMPARE(branches.size(), 1);
    QVERIFY(branches.first().toString().startsWith(QStringLiteral("default")));

    // add branch
    j = m_proxy->branch(QUrl::fromLocalFile(mercurialTest_BaseDir), VcsRevision::createSpecialRevision(VcsRevision::Head), "test");
    verifyJobSucceed(j);
    j = m_proxy->commit(QString("commit 0"), {QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);
    j = m_proxy->branches(QUrl::fromLocalFile(mercurialTest_BaseDir));
    verifyJobSucceed(j);
    branches = j->fetchResults().toList();
    std::sort(branches.begin(), branches.end());
    QCOMPARE(branches.size(), 2);
    QVERIFY(branches.first().toString().startsWith(QStringLiteral("default")));
    QVERIFY(branches.last().toString().startsWith(QStringLiteral("test")));

    // switch branch
    j = m_proxy->currentBranch(QUrl::fromLocalFile(mercurialTest_BaseDir));
    verifyJobSucceed(j);
    branches = j->fetchResults().toList();
    QCOMPARE(branches.size(), 1);
    QVERIFY(branches.first().toString().startsWith(QStringLiteral("test")));
    j = m_proxy->switchBranch(QUrl::fromLocalFile(mercurialTest_BaseDir), QStringLiteral("default"));
    verifyJobSucceed(j);
    j = m_proxy->currentBranch(QUrl::fromLocalFile(mercurialTest_BaseDir));
    verifyJobSucceed(j);
    branches = j->fetchResults().toList();
    QCOMPARE(branches.size(), 1);
    QVERIFY(branches.first().toString().startsWith(QStringLiteral("default")));

    // commit + merge
    writeToFile(mercurialTest_BaseDir + mercurialTest_FileName, "commit 2 content");
    j = m_proxy->commit(QString("commit 2"), {QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);
    j = m_proxy->switchBranch(QUrl::fromLocalFile(mercurialTest_BaseDir), QStringLiteral("test"));
    verifyJobSucceed(j);
    j = m_proxy->mergeBranch(QUrl::fromLocalFile(mercurialTest_BaseDir), QStringLiteral("default"));
    verifyJobSucceed(j);
    j = m_proxy->commit(QString("Merge default"), {QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);
    auto commits = m_proxy->getAllCommits(mercurialTest_BaseDir);
    QCOMPARE(commits.count(), 5);
    QCOMPARE(commits[0].log(), QString("Merge default"));
    QCOMPARE(commits[0].parents().size(), 2);
}

void MercurialTest::testRevisionHistory()
{
    repoInit();
    addFiles();
    commitFiles();

    const auto commits = m_proxy->getAllCommits(mercurialTest_BaseDir);
    QCOMPARE(commits.count(), 2);
    QCOMPARE(commits[0].parents().size(), 1); //initial commit is on the top
    QVERIFY(commits[1].parents().isEmpty());  //0 is later than 1!
    QCOMPARE(commits[0].log(), QString("commit 1"));  //0 is later than 1!
    QCOMPARE(commits[1].log(), QString("commit 0"));
    QVERIFY(commits[1].commit().contains(QRegExp("^\\w{,40}$")));
    QVERIFY(commits[0].commit().contains(QRegExp("^\\w{,40}$")));
    QVERIFY(commits[0].parents()[0].contains(QRegExp("^\\w{,40}$")));
}

void MercurialTest::removeTempDirs()
{
    if (QFileInfo(mercurialTest_BaseDir).exists())
        if (!(KIO::del(QUrl::fromLocalFile(mercurialTest_BaseDir))->exec()))
            qCDebug(PLUGIN_MERCURIAL) << "KIO::del(" << mercurialTest_BaseDir << ") returned false";

    if (QFileInfo(mercurialTest_BaseDir2).exists())
        if (!(KIO::del(QUrl::fromLocalFile(mercurialTest_BaseDir2))->exec()))
            qCDebug(PLUGIN_MERCURIAL) << "KIO::del(" << mercurialTest_BaseDir2 << ") returned false";
}

void MercurialTest::testAnnotate()
{
    repoInit();
    addFiles();
    commitFiles();

    // TODO: Check annotation with a lot of commits (> 200)
    VcsRevision revision;
    revision.setRevisionValue(0, KDevelop::VcsRevision::GlobalNumber);
    auto job = m_proxy->annotate(QUrl::fromLocalFile(mercurialTest_BaseDir + mercurialTest_FileName), revision);
    verifyJobSucceed(job);

    auto results = job->fetchResults().toList();
    QCOMPARE(results.size(), 2);

    auto commit0 = results[0].value<VcsAnnotationLine>();
    QCOMPARE(commit0.text(), QStringLiteral("commit 0 content"));
    QCOMPARE(commit0.lineNumber(), 0);
    QVERIFY(commit0.date().isValid());
    QCOMPARE(commit0.revision().revisionValue().toLongLong(), 0);

    auto commit1 = results[1].value<VcsAnnotationLine>();
    QCOMPARE(commit1.text(), QStringLiteral("commit 1 content"));
    QCOMPARE(commit1.lineNumber(), 1);
    QVERIFY(commit1.date().isValid());
    QCOMPARE(commit1.revision().revisionValue().toLongLong(), 1);

    // Let's change a file without commiting it
    writeToFile(mercurialTest_BaseDir + mercurialTest_FileName, "commit 2 content (temporary)", QIODevice::Append);

    job = m_proxy->annotate(QUrl::fromLocalFile(mercurialTest_BaseDir + mercurialTest_FileName), revision);
    verifyJobSucceed(job);

    results = job->fetchResults().toList();
    QCOMPARE(results.size(), 3);
    auto commit2 = results[2].value<VcsAnnotationLine>();
    QCOMPARE(commit2.text(), QStringLiteral("commit 2 content (temporary)"));
    QCOMPARE(commit2.lineNumber(), 2);
    QVERIFY(commit2.date().isValid());
    QCOMPARE(commit2.revision().revisionValue().toLongLong(), 2);
    QCOMPARE(commit2.author(), QStringLiteral("not.committed.yet"));
    QCOMPARE(commit2.commitMessage(), QStringLiteral("Not Committed Yet"));

    // Check that commit 2 is stripped and mercurialTest_FileName is still modified
    job = m_proxy->status({QUrl::fromLocalFile(mercurialTest_BaseDir + mercurialTest_FileName)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(job);

    auto statusResults = job->fetchResults().toList();
    QCOMPARE(statusResults.size(), 1);
    auto status = statusResults[0].value<VcsStatusInfo>();
    QCOMPARE(status.url(), QUrl::fromLocalFile(mercurialTest_BaseDir + mercurialTest_FileName));
    QCOMPARE(status.state(), VcsStatusInfo::ItemModified);
}

void MercurialTest::testDiff()
{
    repoInit();
    addFiles();
    commitFiles();

    writeToFile(mercurialTest_BaseDir + mercurialTest_FileName, "commit 2 content (temporary)", QIODevice::Append);

    VcsRevision srcrev = VcsRevision::createSpecialRevision(VcsRevision::Base);
    VcsRevision dstrev = VcsRevision::createSpecialRevision(VcsRevision::Working);
    VcsJob* j = m_proxy->diff(QUrl::fromLocalFile(mercurialTest_BaseDir), srcrev, dstrev, IBasicVersionControl::Recursive);
    verifyJobSucceed(j);

    KDevelop::VcsDiff d = j->fetchResults().value<KDevelop::VcsDiff>();
    QCOMPARE(d.baseDiff().toLocalFile(), mercurialTest_BaseDir.left(mercurialTest_BaseDir.size() - 1));
    QVERIFY(d.diff().contains(QUrl::fromLocalFile(mercurialTest_BaseDir + mercurialTest_FileName).toLocalFile()));
}

void MercurialTest::testAnnotateFailed()
{
    repoInit();
    addFiles();
    commitFiles();

    auto verifyAnnotateJobFailed = [this](MercurialAnnotateJob::TestCase test)
    {
        VcsRevision revision;
        revision.setRevisionValue(0, KDevelop::VcsRevision::GlobalNumber);
        auto job = m_proxy->annotate(QUrl::fromLocalFile(mercurialTest_BaseDir + mercurialTest_FileName), revision);
        auto annotateJob = dynamic_cast<MercurialAnnotateJob*>(job);
        QVERIFY(annotateJob);
        annotateJob->m_testCase = test;

        QVERIFY(job->exec());
        QVERIFY(job->status() == KDevelop::VcsJob::JobFailed);
    };
    verifyAnnotateJobFailed(MercurialAnnotateJob::TestCase::Status);
    //verifyAnnotateJobFailed(MercurialAnnotateJob::TestCase::Commit);
    verifyAnnotateJobFailed(MercurialAnnotateJob::TestCase::Annotate);
    verifyAnnotateJobFailed(MercurialAnnotateJob::TestCase::Log);
    //verifyAnnotateJobFailed(MercurialAnnotateJob::TestCase::Strip);
}

void MercurialTest::testHeads()
{
    repoInit();
    addFiles();
    commitFiles();

    VcsRevision revision;
    revision.setRevisionValue(0, KDevelop::VcsRevision::GlobalNumber);
    auto job = m_proxy->checkoutHead(QUrl::fromLocalFile(mercurialTest_BaseDir), revision);
    verifyJobSucceed(job);

    writeToFile(mercurialTest_BaseDir + mercurialTest_FileName, "new head content", QIODevice::Append);

    auto j = m_proxy->commit(QString("new head"), {QUrl::fromLocalFile(mercurialTest_BaseDir)}, KDevelop::IBasicVersionControl::Recursive);
    verifyJobSucceed(j);

    QScopedPointer<MercurialHeadsModel> headsModel(new MercurialHeadsModel(m_proxy, QUrl::fromLocalFile(mercurialTest_BaseDir), nullptr));
    headsModel->update();

    QSignalSpy waiter(headsModel.data(), &MercurialHeadsModel::updateComplete);
    QVERIFY(waiter.wait());
    QVERIFY(waiter.count() == 1);

    QCOMPARE(headsModel->rowCount(), 2);
    auto rev2 = headsModel->data(headsModel->index(0, VcsBasicEventModel::RevisionColumn)).toLongLong();
    auto rev1 = headsModel->data(headsModel->index(1, VcsBasicEventModel::RevisionColumn)).toLongLong();
    QCOMPARE(rev2, 2);
    QCOMPARE(rev1, 1);
}

QTEST_MAIN(MercurialTest)
