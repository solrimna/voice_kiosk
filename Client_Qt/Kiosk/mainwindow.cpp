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
    , mediaPlayer(new QMediaPlayer(this))       // tts 재생용
    , audioOutput(new QAudioOutput(this))       // tts 재생용
{
    ui->setupUi(this);

    // 버튼 클릭 이벤트 연결
    connect(ui->pushButton, &QPushButton::clicked,
            this, &MainWindow::onStartButtonClicked);

    // 네트워크 응답 이벤트 연결(메뉴 조회용)
    connect(networkManager, &QNetworkAccessManager::finished,
            this, &MainWindow::onNetworkReply);

    // 녹음 버튼 연결
    connect(ui->btnStartRecord, &QPushButton::clicked,
            this, &MainWindow::onStartRecord);
    connect(ui->btnStopRecord, &QPushButton::clicked,
            this, &MainWindow::onStopRecord);

    // Azure Speech 테스트 버튼 연결
    connect(ui->btnTestAzure, &QPushButton::clicked,
            this, &MainWindow::onTestAzureSpeech);

    // 오디오 녹음 설정
    captureSession->setAudioInput(audioInput);
    captureSession->setRecorder(mediaRecorder);

    // 오디오 출력 설정
    mediaPlayer->setAudioOutput(audioOutput);
    audioOutput->setVolume(1.0);  // 볼륨 최대

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

        QString userMessage = obj["user_message"].toString();  // STT 결과
        QString aiResponse = obj["ai_response"].toString();    // ai 응답
        QString ttsUrl = obj["tts_url"].toString();             // TTS URL

        qDebug() << "사용자:" << userMessage;
        qDebug() << "AI:" << aiResponse;
        qDebug() << "TTS URL:" << ttsUrl;

        // 대화 형식으로 표시
        QString resultMsg = QString("👤 고객님:\n%1\n\n🤖 AI 도우미:\n%2\n\n🔊 음성으로 재생 중...")
                                .arg(userMessage)
                                .arg(aiResponse);

        QMessageBox::information(this, "AI 키오스크", resultMsg);

        // 파일 이름 & 사이즈는 확인했으므로 주석 처리 =======================
        //
        // QString fileName = obj["file_name"].toString();
        // int fileSize = obj["file_size"].toInt();

        // 디버그: 개별 값 출력
        // qDebug() << "message:" << message;
        // qDebug() << "fileName:" << fileName;
        // qDebug() << "fileSize:" << fileSize;
        // qDebug() << "STT 결과:" << user_message;
        // QString resultMsg = QString("%1\n파일명: %2\n크기: %3 bytes\n\n 음성인식 결과:\n「%4」")
        //                         .arg(message,
        //                              fileName,
        //                              QString::number(fileSize),
        //                              user_message.isEmpty() ? "인식 실패" : transcription);
        // ================================================================

        // TTS 음성 재생! (새로 추가!)
        if (!ttsUrl.isEmpty()) {
            QString fullUrl = "http://127.0.0.1:8000" + ttsUrl;
            qDebug() << "TTS 재생:" << fullUrl;

            mediaPlayer->setSource(QUrl(fullUrl));
            mediaPlayer->play();
        }

    } else {
        QMessageBox::critical(this, "오류", reply->errorString());
    }

    reply->deleteLater();
}

void MainWindow::onTestAzureSpeech()
{
    using namespace Microsoft::CognitiveServices::Speech;
    using namespace Microsoft::CognitiveServices::Speech::Audio;

    qDebug() << "=== Azure Speech 테스트 시작 ===";

    // 환경변수에서 API 키 가져오기
    QString apiKey = qgetenv("AZURE_SPEECH_KEY");
    QString region = qgetenv("AZURE_SPEECH_REGION");

    // 환경변수 확인
    if (apiKey.isEmpty() || region.isEmpty()) {
        QMessageBox::critical(this, "환경변수 오류",
                              "환경변수가 설정되지 않았습니다!\n\n"
                              "필요한 환경변수:\n"
                              "- AZURE_SPEECH_KEY\n"
                              "- AZURE_SPEECH_REGION\n\n"
                              "Qt Creator의 Projects → Run → Environment에서 설정하세요.");
        return;
    }

    qDebug() << "API Key 확인: (길이:" << apiKey.length() << ")";
    qDebug() << "Region:" << region;

    try {
        // Speech Config 생성
        auto config = SpeechConfig::FromSubscription(
            apiKey.toStdString(),
            region.toStdString()
            );

        // 한국어 설정
        config->SetSpeechRecognitionLanguage("ko-KR");

        qDebug() << "Speech Config 생성 완료";

        // 오디오 설정 (기본 마이크)
        auto audioConfig = AudioConfig::FromDefaultMicrophoneInput();

        qDebug() << "Audio Config 생성 완료";

        // Speech Recognizer 생성
        auto recognizer = SpeechRecognizer::FromConfig(config, audioConfig);

        qDebug() << "Speech Recognizer 생성 완료";

        // 사용자에게 안내
        QMessageBox::information(this, "준비 완료",
                                 "🎤 마이크가 준비되었습니다!\n\n"
                                 "OK 버튼을 누르면 5초 안에 말씀하세요.\n\n"
                                 "예시:\n"
                                 "- 안녕하세요\n"
                                 "- 아메리카노 주세요\n"
                                 "- 테스트입니다");

        qDebug() << "음성 인식 시작...";

        // 음성 인식 (한 번, 5초 타임아웃, get() 결과가 올 때까지 대기)
        auto result = recognizer->RecognizeOnceAsync().get();

        qDebug() << "음성 인식 완료";

        // 결과 처리
        if (result->Reason == ResultReason::RecognizedSpeech) {
            QString text = QString::fromStdString(result->Text);
            qDebug() << "✅ 인식 성공:" << text;

            QMessageBox::information(this, "인식 성공!",
                                     QString("✅ 인식된 텍스트:\n\n「%1」\n\n"
                                             "Azure Speech 연동 성공!").arg(text));
        }
        // 인식 실패(소음 또는 너무 작은 소리 일 때)
        else if (result->Reason == ResultReason::NoMatch) {
            qDebug() << "❌ 음성 인식 실패";

            QMessageBox::warning(this, "인식 실패",
                                 "음성을 인식하지 못했습니다.\n\n"
                                 "다시 시도해주세요.\n"
                                 "마이크가 제대로 연결되어 있는지 확인하세요.");
        }
        // 오류 (API키 틀림, 네트워크 문제 etc...)
        else if (result->Reason == ResultReason::Canceled) {
            auto cancellation = CancellationDetails::FromResult(result);
            QString errorDetails = QString::fromStdString(cancellation->ErrorDetails);

            qDebug() << "❌ 취소됨:" << errorDetails;

            QMessageBox::critical(this, "오류 발생",
                                  QString("음성 인식이 취소되었습니다.\n\n"
                                          "오류: %1\n\n"
                                          "API 키와 지역(Region)을 확인하세요.").arg(errorDetails));
        }

    }
    catch (const std::exception& e) {
        QString error = QString::fromStdString(e.what());
        qDebug() << "❌ 예외 발생:" << error;

        QMessageBox::critical(this, "예외 발생",
                              QString("예외가 발생했습니다:\n\n%1").arg(error));
    }

    qDebug() << "=== Azure Speech 테스트 종료 ===";
}
