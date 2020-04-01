/*=========================================================================

   Program: ParaView
   Module:  pqLogViewerWindow.cxx

   Copyright (c) 2005,2006 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.2.

   See License_v1.2.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

========================================================================*/

#include "pqLogViewerWindow.h"

#include "pqActiveObjects.h"
#include "pqServer.h"

#include <QMouseEvent>

#include "vtkPVLogInformation.h"
#include "vtkPVLogger.h"
#include "vtkPVServerInformation.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMSession.h"
#include "vtkSMSessionProxyManager.h"

#include "ui_pqLogViewerWindow.h"

namespace
{
std::map<int, vtkLogger::Verbosity> indexToVerbosity;
std::map<vtkLogger::Verbosity, int> verbosityToIndex;
constexpr int PIXMAP_SIZE = 16;

constexpr int CLIENT_PROCESS = 0;
constexpr int SERVER_PROCESS = 1;
constexpr int DATA_SERVER_PROCESS = 1;
constexpr int RENDER_SERVER_PROCESS = 2;

constexpr int DATA_MOVEMENT_CATEGORY = 0;
constexpr int RENDERING_CATEGORY = 1;
constexpr int APPLICATION_CATEGORY = 2;
constexpr int PIPELINE_CATEGORY = 3;
constexpr int PLUGINS_CATEGORY = 4;
}

//----------------------------------------------------------------------------
pqLogViewerWindow::pqLogViewerWindow()
{
  this->Ui = new Ui::pqLogViewerWindow();
  this->Ui->setupUi(this);

  indexToVerbosity[0] = vtkLogger::VERBOSITY_OFF;
  indexToVerbosity[1] = vtkLogger::VERBOSITY_ERROR;
  indexToVerbosity[2] = vtkLogger::VERBOSITY_WARNING;
  indexToVerbosity[3] = vtkLogger::VERBOSITY_INFO;
  indexToVerbosity[4] = vtkLogger::VERBOSITY_TRACE;
  indexToVerbosity[5] = vtkLogger::VERBOSITY_0;
  indexToVerbosity[6] = vtkLogger::VERBOSITY_1;
  indexToVerbosity[7] = vtkLogger::VERBOSITY_2;
  indexToVerbosity[8] = vtkLogger::VERBOSITY_3;
  indexToVerbosity[9] = vtkLogger::VERBOSITY_4;
  indexToVerbosity[10] = vtkLogger::VERBOSITY_5;
  indexToVerbosity[11] = vtkLogger::VERBOSITY_6;
  indexToVerbosity[12] = vtkLogger::VERBOSITY_7;
  indexToVerbosity[13] = vtkLogger::VERBOSITY_8;
  indexToVerbosity[14] = vtkLogger::VERBOSITY_9;

  verbosityToIndex[vtkLogger::VERBOSITY_OFF] = 0;
  verbosityToIndex[vtkLogger::VERBOSITY_ERROR] = 1;
  verbosityToIndex[vtkLogger::VERBOSITY_WARNING] = 2;
  verbosityToIndex[vtkLogger::VERBOSITY_INFO] = 3;
  verbosityToIndex[vtkLogger::VERBOSITY_TRACE] = 4;
  // verbosityToIndex[vtkLogger::VERBOSITY_0] = 5; same as INFO, prefer INFO
  verbosityToIndex[vtkLogger::VERBOSITY_1] = 6;
  verbosityToIndex[vtkLogger::VERBOSITY_2] = 7;
  verbosityToIndex[vtkLogger::VERBOSITY_3] = 8;
  verbosityToIndex[vtkLogger::VERBOSITY_4] = 9;
  verbosityToIndex[vtkLogger::VERBOSITY_5] = 10;
  verbosityToIndex[vtkLogger::VERBOSITY_6] = 11;
  verbosityToIndex[vtkLogger::VERBOSITY_7] = 12;
  verbosityToIndex[vtkLogger::VERBOSITY_8] = 13;
  // verbosityToIndex[vtkLogger::VERBOSITY_9] = 14; same as TRACE, prefer TRACE

  for (size_t i = 0; i < this->CategoryPromoted.size(); ++i)
  {
    this->CategoryPromoted[i] = false;
  }

  auto* server = pqActiveObjects::instance().activeServer();
  auto* pxm = server->proxyManager();
  this->LogRecorderProxies.push_back(pxm->NewProxy("misc", "LogRecorder"));
  this->LogRecorderProxies[CLIENT_PROCESS]->SetLocation(vtkSMSession::CLIENT);
  vtkSMPropertyHelper(this->LogRecorderProxies[CLIENT_PROCESS], "RankEnabled").Set(0);
  this->LogRecorderProxies[CLIENT_PROCESS]->UpdateVTKObjects();

  this->Ui->processComboBox->addItem("Client");
  vtkNew<vtkPVServerInformation> serverInfo;

  if (server->isRemote())
  {
    if (server->isRenderServerSeparate())
    {
      this->LogRecorderProxies.push_back(pxm->NewProxy("misc", "LogRecorder"));
      this->LogRecorderProxies[DATA_SERVER_PROCESS]->SetLocation(vtkSMSession::DATA_SERVER);
      this->LogRecorderProxies[DATA_SERVER_PROCESS]->UpdateVTKObjects();

      this->LogRecorderProxies.push_back(pxm->NewProxy("misc", "LogRecorder"));
      this->LogRecorderProxies[RENDER_SERVER_PROCESS]->SetLocation(vtkSMSession::RENDER_SERVER);
      this->LogRecorderProxies[RENDER_SERVER_PROCESS]->UpdateVTKObjects();

      this->Ui->processComboBox->addItem("Data Server");
      this->Ui->processComboBox->addItem("Render Server");

      server->session()->GatherInformation(DATA_SERVER_PROCESS, serverInfo, 0);
      this->RankNumbers.push_back(serverInfo->GetNumberOfProcesses());
      server->session()->GatherInformation(RENDER_SERVER_PROCESS, serverInfo, 0);
      this->RankNumbers.push_back(serverInfo->GetNumberOfProcesses());
    }
    else
    {
      this->LogRecorderProxies.push_back(pxm->NewProxy("misc", "LogRecorder"));
      this->LogRecorderProxies[SERVER_PROCESS]->SetLocation(vtkSMSession::SERVERS);
      this->LogRecorderProxies[SERVER_PROCESS]->UpdateVTKObjects();

      this->Ui->processComboBox->addItem("Server");

      server->session()->GatherInformation(vtkPVSession::SERVERS, serverInfo, 0);
      this->RankNumbers.push_back(serverInfo->GetNumberOfProcesses());
    }
  }

  server->session()->GatherInformation(vtkPVSession::CLIENT, serverInfo, 0);
  this->RankNumbers.push_back(serverInfo->GetNumberOfProcesses());

  this->recordRefTimes();

  this->appendLogView(new pqSingleLogViewerWidget(this, this->LogRecorderProxies[0], 0));

  this->Ui->processComboBox->setCurrentIndex(0);

  this->initializeRankComboBox();
  this->initializeVerbosityComboBoxes();

  QObject::connect(this->Ui->processComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
    this, &pqLogViewerWindow::initializeRankComboBox);
  QObject::connect(
    this->Ui->addLogButton, &QPushButton::pressed, this, &pqLogViewerWindow::addLogView);
  QObject::connect(this->Ui->clientVerbosities, QOverload<int>::of(&QComboBox::currentIndexChanged),
    [=](int index) { this->setProcessVerbosity(CLIENT_PROCESS, index); });
  QObject::connect(this->Ui->serverVerbosities, QOverload<int>::of(&QComboBox::currentIndexChanged),
    [=](int index) { this->setProcessVerbosity(SERVER_PROCESS, index); });
  QObject::connect(this->Ui->dataServerVerbosities,
    QOverload<int>::of(&QComboBox::currentIndexChanged),
    [=](int index) { this->setProcessVerbosity(DATA_SERVER_PROCESS, index); });
  QObject::connect(this->Ui->renderServerVerbosities,
    QOverload<int>::of(&QComboBox::currentIndexChanged),
    [=](int index) { this->setProcessVerbosity(RENDER_SERVER_PROCESS, index); });
  QObject::connect(
    this->Ui->refreshButton, &QPushButton::pressed, this, &pqLogViewerWindow::refresh);
  QObject::connect(
    this->Ui->clearLogsButton, &QPushButton::pressed, this, &pqLogViewerWindow::clear);
  QObject::connect(this->Ui->logTabWidget, &QTabWidget::tabCloseRequested, this->Ui->logTabWidget,
    &QTabWidget::removeTab);

  this->Ui->clientVerbosities->setCurrentIndex(
    getVerbosityIndex(vtkLogger::GetCurrentVerbosityCutoff()));

  if (this->LogRecorderProxies.size() == 2)
  {
    this->Ui->dataServerVerbosityLabel->setVisible(false);
    this->Ui->dataServerVerbosities->setVisible(false);
    this->Ui->renderServerVerbosityLabel->setVisible(false);
    this->Ui->renderServerVerbosities->setVisible(false);
    vtkNew<vtkPVLogInformation> serverLogInfo;
    this->LogRecorderProxies[SERVER_PROCESS]->GatherInformation(serverLogInfo);
    this->Ui->serverVerbosities->setCurrentIndex(
      getVerbosityIndex(static_cast<vtkLogger::Verbosity>(serverLogInfo->GetVerbosity())));
  }
  else if (this->LogRecorderProxies.size() == 3)
  {
    this->Ui->serverVerbosities->setVisible(false);
    this->Ui->serverVerbosityLabel->setVisible(false);
    vtkNew<vtkPVLogInformation> serverLogInfo;
    this->LogRecorderProxies[DATA_SERVER_PROCESS]->GatherInformation(serverLogInfo);
    this->Ui->dataServerVerbosities->setCurrentIndex(
      getVerbosityIndex(static_cast<vtkLogger::Verbosity>(serverLogInfo->GetVerbosity())));
    this->LogRecorderProxies[RENDER_SERVER_PROCESS]->GatherInformation(serverLogInfo);
    this->Ui->renderServerVerbosities->setCurrentIndex(
      getVerbosityIndex(static_cast<vtkLogger::Verbosity>(serverLogInfo->GetVerbosity())));
  }
  else
  {
    this->Ui->dataServerVerbosityLabel->setVisible(false);
    this->Ui->dataServerVerbosities->setVisible(false);
    this->Ui->renderServerVerbosityLabel->setVisible(false);
    this->Ui->renderServerVerbosities->setVisible(false);
    this->Ui->serverVerbosities->setVisible(false);
    this->Ui->serverVerbosityLabel->setVisible(false);
  }

  // Set up category checkbox connections
  QObject::connect(this->Ui->dataMovementCheckBox, &QCheckBox::stateChanged,
    [=](bool checked) { this->updateCategory(DATA_MOVEMENT_CATEGORY, checked); });
  QObject::connect(this->Ui->renderingCheckBox, &QCheckBox::stateChanged,
    [=](bool checked) { this->updateCategory(RENDERING_CATEGORY, checked); });
  QObject::connect(this->Ui->applicationCheckBox, &QCheckBox::stateChanged,
    [=](bool checked) { this->updateCategory(APPLICATION_CATEGORY, checked); });
  QObject::connect(this->Ui->pipelineCheckBox, &QCheckBox::stateChanged,
    [=](bool checked) { this->updateCategory(PIPELINE_CATEGORY, checked); });
  QObject::connect(this->Ui->pluginsCheckBox, &QCheckBox::stateChanged,
    [=](bool checked) { this->updateCategory(PLUGINS_CATEGORY, checked); });
}

//----------------------------------------------------------------------------
pqLogViewerWindow::~pqLogViewerWindow()
{
  for (auto logRecorderProxy : this->LogRecorderProxies)
  {
    logRecorderProxy->Delete();
  }
}

//----------------------------------------------------------------------------
void pqLogViewerWindow::refresh()
{
  for (auto* logView : this->LogViews)
  {
    logView->refresh();
  }
}

//----------------------------------------------------------------------------
void pqLogViewerWindow::clear()
{
  for (auto logRecorderProxy : this->LogRecorderProxies)
  {
    logRecorderProxy->InvokeCommand("ClearLogs");
  }

  for (auto* logView : this->LogViews)
  {
    logView->setLog("");
  }
}

//----------------------------------------------------------------------------
void pqLogViewerWindow::addLogView()
{
  int serverIndex = this->Ui->processComboBox->currentIndex();
  int rankIndex = this->Ui->rankComboBox->currentText().toInt();

  this->appendLogView(
    new pqSingleLogViewerWidget(this, this->LogRecorderProxies[serverIndex], rankIndex));
}

//----------------------------------------------------------------------------
void pqLogViewerWindow::recordRefTimes()
{
  for (int i = 0; i < this->LogRecorderProxies.size(); i++)
  {
    for (int j = 0; j < this->RankNumbers[i]; j++)
    {
      vtkNew<vtkPVLogInformation> refTimeInfo;
      refTimeInfo->SetRank(j);
      this->LogRecorderProxies[i]->GatherInformation(refTimeInfo);
      auto startingLog = QString::fromStdString(refTimeInfo->GetStartingLogs());
      bool isRawLog;
      auto parts = pqLogViewerWidget::extractLogParts(&startingLog, isRawLog);
      this->RefTimes[LogLocation(this->LogRecorderProxies[i], j)] =
        parts[0].replace('s', '0').toDouble();
    }
  }
}

//----------------------------------------------------------------------------
bool pqLogViewerWindow::eventFilter(QObject* object, QEvent* event)
{
  if (event->type() == QEvent::MouseButtonRelease && qobject_cast<QLabel*>(object))
  {
    QMouseEvent* mouseEvent = dynamic_cast<QMouseEvent*>(event);
    if (mouseEvent->button() == Qt::LeftButton)
    {
      for (int cc = 0; cc < this->Ui->logTabWidget->count(); ++cc)
      {
        QWidget* tab = this->Ui->logTabWidget->tabBar()->tabButton(cc, QTabBar::RightSide);
        if (tab == qobject_cast<QWidget*>(object))
        {
          this->Ui->logTabWidget->removeTab(cc);
          break;
        }
      }
    }
  }

  return this->Superclass::eventFilter(object, event);
}

//----------------------------------------------------------------------------
void pqLogViewerWindow::appendLogView(pqSingleLogViewerWidget* logView)
{
  QString tabTitle;
  int rank = logView->getRank();
  auto serverLocation = logView->getLogRecorderProxy()->GetLocation();
  switch (serverLocation)
  {
    case vtkSMSession::CLIENT:
      tabTitle = QString("Client - Rank ") + QString::number(rank);
      break;
    case vtkSMSession::SERVERS:
      tabTitle = QString("Server - Rank ") + QString::number(rank);
      break;
    case vtkSMSession::DATA_SERVER:
      tabTitle = QString("Data Server - Rank ") + QString::number(rank);
      break;
    case vtkSMSession::RENDER_SERVER:
      tabTitle = QString("Render Server - Rank ") + QString::number(rank);
      break;
    default:
      break;
  }

  // Set up custom icons on the tab
  int tabIndex = this->Ui->logTabWidget->addTab(logView, tabTitle);
  QLabel* label = new QLabel();
  label->setObjectName("close");
  label->setToolTip("Close log");
  label->setStatusTip("Close log");
  label->setPixmap(
    label->style()->standardIcon(QStyle::SP_TitleBarCloseButton).pixmap(PIXMAP_SIZE, PIXMAP_SIZE));
  this->Ui->logTabWidget->tabBar()->setTabButton(tabIndex, QTabBar::RightSide, label);
  label->installEventFilter(this);

  this->LogViews.append(logView);
  QObject::connect(Ui->globalFilter, &QLineEdit::textChanged, logView,
    &pqSingleLogViewerWidget::setFilterWildcard);

  QObject::connect(logView, &pqLogViewerWidget::scrolled, this, &pqLogViewerWindow::linkedScroll);
}

//----------------------------------------------------------------------------
void pqLogViewerWindow::initializeRankComboBox()
{
  int index = this->Ui->processComboBox->currentIndex();

  this->Ui->rankComboBox->clear();
  for (int i = 0; i < this->RankNumbers[index]; i++)
  {
    this->Ui->rankComboBox->addItem(QString::number(i));
  }
}

//----------------------------------------------------------------------------
void pqLogViewerWindow::initializeVerbosityComboBoxes()
{
  this->initializeVerbosities(this->Ui->clientVerbosities);
  if (this->LogRecorderProxies.size() == 2)
  {
    this->initializeVerbosities(this->Ui->serverVerbosities);
  }
  else if (this->LogRecorderProxies.size() == 3)
  {
    this->initializeVerbosities(this->Ui->dataServerVerbosities);
    this->initializeVerbosities(this->Ui->renderServerVerbosities);
  }
}

//----------------------------------------------------------------------------
void pqLogViewerWindow::initializeVerbosities(QComboBox* combobox)
{
  combobox->addItem("OFF");
  combobox->addItem("ERROR");
  combobox->addItem("WARNING");
  combobox->addItem("INFO");
  combobox->addItem("TRACE");

  for (int i = 0; i <= 9; i++)
  {
    combobox->addItem(QString::number(i));
  }
}

//----------------------------------------------------------------------------
void pqLogViewerWindow::updateCategory(int category, bool promote)
{
  this->CategoryPromoted[static_cast<int>(category)] = promote;

  auto DoUpdate = [=](int proxyIndex, int category) {
    // Reset log messages to INFO
    int verbosity = static_cast<int>(vtkLogger::VERBOSITY_TRACE);
    if (promote)
    {
      // Promote log messages to the chosen verbosity on the process
      verbosity = vtkSMPropertyHelper(this->LogRecorderProxies[proxyIndex], "Verbosity").GetAsInt();
    }

    vtkSMPropertyHelper(this->LogRecorderProxies[proxyIndex], "CategoryVerbosity").Set(0, category);
    vtkSMPropertyHelper(this->LogRecorderProxies[proxyIndex], "CategoryVerbosity")
      .Set(1, verbosity);
    this->LogRecorderProxies[proxyIndex]->UpdateVTKObjects();
  };

  for (int proxyIndex = 0; proxyIndex < this->LogRecorderProxies.size(); ++proxyIndex)
  {
    DoUpdate(proxyIndex, category);
  }
}

//----------------------------------------------------------------------------
void pqLogViewerWindow::updateCategories()
{
  for (size_t i = 0; i < this->CategoryPromoted.size(); ++i)
  {
    this->updateCategory(i, this->CategoryPromoted[i]);
  }
}

//----------------------------------------------------------------------------
vtkLogger::Verbosity pqLogViewerWindow::getVerbosity(int index)
{
  if (index >= 0 && index <= 15)
  {
    return indexToVerbosity[index];
  }

  return vtkPVLogger::GetDefaultVerbosity();
}

//----------------------------------------------------------------------------
int pqLogViewerWindow::getVerbosityIndex(vtkLogger::Verbosity verbosity)
{
  return verbosityToIndex[verbosity];
}

//----------------------------------------------------------------------------
void pqLogViewerWindow::linkedScroll(double time)
{
  auto* sender = static_cast<pqSingleLogViewerWidget*>(QObject::sender());
  double timeDelta =
    time - this->RefTimes[LogLocation(sender->getLogRecorderProxy(), sender->getRank())];
  for (auto logView : this->LogViews)
  {
    if (logView != sender && logView->isVisible())
    {
      double realTime =
        this->RefTimes[LogLocation(logView->getLogRecorderProxy(), logView->getRank())] + timeDelta;
      logView->scrollToTime(realTime);
    }
  }
}

//----------------------------------------------------------------------------
void pqLogViewerWindow::setProcessVerbosity(int process, int index)
{
  int verbosity = static_cast<int>(this->getVerbosity(index));
  vtkSMPropertyHelper(this->LogRecorderProxies[process], "Verbosity").Set(verbosity);
  this->LogRecorderProxies[process]->UpdateProperty("Verbosity");

  // Update the category verbosity
  this->updateCategories();
}
