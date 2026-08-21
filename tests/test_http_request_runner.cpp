#include <QCoreApplication>
#include <QNetworkReply>
#include <QTest>

#include "HttpRequestRunner.h"

class TestHttpRequestRunner : public QObject {
    Q_OBJECT

private slots:
    void testRequestPolicySuccess() {
        RequestPolicy policy;
        GroqApiResponse res;
        res.isSuccess = true;
        res.httpStatus = 200;

        QCOMPARE(policy.isTransientError(res), false);
        QCOMPARE(policy.shouldRetry(res, 0, false), false);
    }

    void testRequestPolicyTransientRateLimit() {
        RequestPolicy policy;
        GroqApiResponse res;
        res.isSuccess = false;
        res.httpStatus = 429;
        res.isRateLimited = true;
        res.retryAfterSeconds = 2;

        QCOMPARE(policy.isTransientError(res), true);
        QCOMPARE(policy.shouldRetry(res, 0, false), true);
        QCOMPARE(policy.calculateRetryDelayMs(res), 2000);

        // Long rate limit (exceeding maxTransientRetryAfterSec)
        res.retryAfterSeconds = 10;
        QCOMPARE(policy.isTransientError(res), false);
        QCOMPARE(policy.shouldRetry(res, 0, false), false);
    }

    void testRequestPolicyServerErrors() {
        RequestPolicy policy;
        const int serverErrors[] = {500, 502, 503, 504, 599};

        for (int code : serverErrors) {
            GroqApiResponse res;
            res.isSuccess = false;
            res.httpStatus = code;

            QCOMPARE(policy.isTransientError(res), true);
            QCOMPARE(policy.shouldRetry(res, 0, false), true);
            QCOMPARE(policy.calculateRetryDelayMs(res), 1000);
        }
    }

    void testRequestPolicyNetworkErrors() {
        RequestPolicy policy;

        const QNetworkReply::NetworkError transientErrors[] = {
            QNetworkReply::RemoteHostClosedError,
            QNetworkReply::TemporaryNetworkFailureError,
            QNetworkReply::TimeoutError,
            QNetworkReply::NetworkSessionFailedError
        };

        for (auto err : transientErrors) {
            GroqApiResponse res;
            res.isSuccess = false;
            res.networkError = err;

            QCOMPARE(policy.isTransientError(res), true);
            QCOMPARE(policy.shouldRetry(res, 0, false), true);
        }

        const QNetworkReply::NetworkError nonTransientErrors[] = {
            QNetworkReply::HostNotFoundError,
            QNetworkReply::ConnectionRefusedError,
            QNetworkReply::OperationCanceledError,
            QNetworkReply::AuthenticationRequiredError
        };

        for (auto err : nonTransientErrors) {
            GroqApiResponse res;
            res.isSuccess = false;
            res.networkError = err;

            QCOMPARE(policy.isTransientError(res), false);
            QCOMPARE(policy.shouldRetry(res, 0, false), false);
        }
    }

    void testRequestPolicyClientErrors() {
        RequestPolicy policy;
        const int clientErrors[] = {400, 401, 403, 404, 422};

        for (int code : clientErrors) {
            GroqApiResponse res;
            res.isSuccess = false;
            res.httpStatus = code;

            QCOMPARE(policy.isTransientError(res), false);
            QCOMPARE(policy.shouldRetry(res, 0, false), false);
        }
    }

    void testRequestPolicyRetryLimitsAndCancellation() {
        RequestPolicy policy;
        GroqApiResponse res;
        res.isSuccess = false;
        res.httpStatus = 503;

        // Valid retry on attempt 0
        QCOMPARE(policy.shouldRetry(res, 0, false), true);

        // Disallowed when max retries (1) reached
        QCOMPARE(policy.shouldRetry(res, 1, false), false);

        // Disallowed when cancelled
        QCOMPARE(policy.shouldRetry(res, 0, true), false);
    }

    void testHttpRequestRunnerLifecycle() {
        HttpRequestRunner runner;
        QCOMPARE(runner.isBusy(), false);
        QCOMPARE(runner.isCancelled(), false);
        QCOMPARE(runner.retryCount(), 0);
        QVERIFY(runner.currentReply().isNull());

        runner.prepareNewRequest();
        QCOMPARE(runner.isBusy(), true);
        QCOMPARE(runner.isCancelled(), false);
        QCOMPARE(runner.retryCount(), 0);

        GroqApiResponse transientRes;
        transientRes.isSuccess = false;
        transientRes.httpStatus = 500;

        QCOMPARE(runner.shouldRetry(transientRes), true);
        QCOMPARE(runner.calculateRetryDelayMs(transientRes), 1000);

        runner.incrementRetryCount();
        QCOMPARE(runner.retryCount(), 1);
        QCOMPARE(runner.shouldRetry(transientRes), false);

        runner.reset();
        QCOMPARE(runner.isBusy(), false);
        QCOMPARE(runner.retryCount(), 0);

        runner.prepareNewRequest();
        runner.cancel();
        QCOMPARE(runner.isBusy(), false);
        QCOMPARE(runner.isCancelled(), true);
        QCOMPARE(runner.shouldRetry(transientRes), false);
    }
};

QTEST_GUILESS_MAIN(TestHttpRequestRunner)
#include "test_http_request_runner.moc"
