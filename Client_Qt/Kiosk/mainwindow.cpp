#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QNetworkRequest>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <QDir>
#include <QFile>           // 파일 전송을 위해 추가
#include <QHttpMultiPart>  // 파일 전송을 위해 추가
#include <QHttpPart>       // 파일 전송을 위해 추가
#include <QFileInfo>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , networkManager(new QNetworkAccessManager(this))
    , uploadManager(new QNetworkAccessManager(this))    // 업로드 전용으로 별도로 만듬
    , captureSession(new QMediaCaptureSession(this))
    , audioInput(new QAudioInput(this))
    , mediaRecorder(new QMediaRecorder(this))
{
    ui->setupUi(this);

    // 버튼 클릭 이벤트 연결
    connect(ui->pushButton, &QPushButton::clicked,
            this, &MainWindow::onStartButtonClicked);

    // 네트워크 응답 이벤트 연결(메뉴 조회용)
    connect(networkManager, &QNetworkAccessManager::finished,
            this, &MainWindow::onNetworkReply);

    // 녹음 버튼 연결 (추가!)
    connect(ui->btnStartRecord, &QPushButton::clicked,
            this, &MainWindow::onStartRecord);
    connect(ui->btnStopRecord, &QPushButton::clicked,
            this, &MainWindow::onStopRecord);

    // 오디오 녹음 설정 (추가!)
    captureSession->setAudioInput(audioInput);
    captureSession->setRecorder(mediaRecorder);

    // 초기 버튼 상태
    ui->btnStopRecord->setEnabled(false);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onStartButtonClicked()
{
    QUrl url("http://127.0.0.1:8000/api/menu/items/");
    QNetworkRequest request(url);

    networkManager->get(request);

    QMessageBox::information(this, "알림", "서버 요청 TEST...");
}

// 네트워크 응답 처리
void MainWindow::onNetworkReply(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        // 성공적으로 응답 받음
        QByteArray response = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(response);
        //QJsonObject obj = doc.object();
        QJsonArray array = doc.array();  // 배열로 받음!

        if (!array.isEmpty()) {
            QJsonObject menu = array[0].toObject();
            QString name = menu["name"].toString();
            int price = menu["price"].toInt();

            QString message = QString("메뉴: %1\n가격: %2원").arg(name).arg(price);
            QMessageBox::information(this, "메뉴 정보", message);
        }
    } else {
        // 에러 발생
        QMessageBox::critical(this, "에러", "서버 통신 실패: " + reply->errorString());
    }

    reply->deleteLater();
}
// 녹음 시작 버튼
void MainWindow::onStartRecord()
{
    // 저장 경로 설정
    // Qt 6 방식 (Qt 5는 QStandardPaths::DocumentsPath로 해야함)
    QString documentsPath = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString audioPath = documentsPath + "/KioskAudio";

    // 폴더 생성
    QDir dir;
    if (!dir.exists(audioPath)) {
        dir.mkpath(audioPath);
    }

    // 파일명 (타임스탬프 포함)
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString fileName = audioPath + "/voice_" + timestamp + ".wav";

    // 파일 경로 저장
    lastRecordedFile = fileName;

    // 녹음 설정
    mediaRecorder->setOutputLocation(QUrl::fromLocalFile(fileName));
    mediaRecorder->setQuality(QMediaRecorder::HighQuality);

    // 녹음 시작
    mediaRecorder->record();

    // 버튼 상태 변경
    ui->btnStartRecord->setEnabled(false);
    ui->btnStopRecord->setEnabled(true);

    QMessageBox::information(this, "녹음", "녹음 시작!\n파일: " + fileName);
}

// 녹음 중지
void MainWindow::onStopRecord()
{
    // 녹음 중지
    mediaRecorder->stop();

    // 버튼 상태 변경
    ui->btnStartRecord->setEnabled(true);
    ui->btnStopRecord->setEnabled(false);

    // 파일 저장시
    // QString savedPath = mediaRecorder->actualLocation().toLocalFile();
    // QMessageBox::information(this, "녹음", "녹음 완료!\n저장 위치:\n" + savedPath);

    // 녹음 완료 후 서버 전송 시
    QMessageBox::information(this, "녹음", "녹음 완료!\n서버로 전송 중...");
    uploadVoiceFile(lastRecordedFile);
}

// 파일 업로드 함수
void MainWindow::uploadVoiceFile(const QString &filePath)
{
    QFile *file = new QFile(filePath);

    if (!file->open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "에러", "파일을 열 수 없습니다: " + filePath);
        delete file;
        return;
    }

    // Multipart 생성
    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    // 파일 파트 생성
    QHttpPart filePart;
    filePart.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("audio/wav"));
    filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                       QVariant("form-data; name=\"voice_file\"; filename=\"" + QFileInfo(filePath).fileName() + "\""));

    filePart.setBodyDevice(file);
    file->setParent(multiPart);  // multiPart 삭제 시 file도 같이 삭제

    multiPart->append(filePart);

    // POST 요청
    QUrl url("http://127.0.0.1:8000/api/voice/upload/");
    QNetworkRequest request(url);

    QNetworkReply *reply = uploadManager->post(request, multiPart);
    multiPart->setParent(reply);  // reply 삭제 시 multiPart도 같이 삭제

    // 업로드 완료 시 처리 - 람다 함수로 reply를 캡처해서 전달
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onUploadFinished(reply);
    });
}

// 업로드 응답 처리
void MainWindow::onUploadFinished(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();

        qDebug() << "Django 응답:" << response;

        QJsonDocument doc = QJsonDocument::fromJson(response);
        QJsonObject obj = doc.object();

        qDebug() << "파싱된 JSON:" << obj;

        QString message = obj["message"].toString();
        QString fileName = obj["file_name"].toString();
        int fileSize = obj["file_size"].toInt();
        QString transcription = obj["transcription"].toString();  // STT 결과!

        // 디버그: 개별 값 출력
        qDebug() << "message:" << message;
        qDebug() << "fileName:" << fileName;
        qDebug() << "fileSize:" << fileSize;
        qDebug() << "STT 결과:" << transcription;

        QString resultMsg = QString("%1\n파일명: %2\n크기: %3 bytes\n\n 음성인식 결과:\n「%4」")
                                .arg(message,
                                     fileName,
                                     QString::number(fileSize),
                                     transcription.isEmpty() ? "인식 실패" : transcription);

        QMessageBox::information(this, "업로드 성공", resultMsg);
    } else {
        QMessageBox::critical(this, "업로드 실패", reply->errorString());
    }

    reply->deleteLater();
}
