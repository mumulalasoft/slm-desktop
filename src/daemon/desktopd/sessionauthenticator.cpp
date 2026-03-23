#include "sessionauthenticator.h"

#include <QProcessEnvironment>
#include <cstdlib>
#include <cstring>

#ifdef SLM_HAVE_PAM
#include <security/pam_appl.h>

namespace {
struct PamConversationData {
    const char *password = nullptr;
};

int pamConversation(int numMsg,
                    const struct pam_message **msg,
                    struct pam_response **resp,
                    void *appdataPtr)
{
    if (numMsg <= 0 || !msg || !resp || !appdataPtr) {
        return PAM_CONV_ERR;
    }

    auto *data = static_cast<PamConversationData *>(appdataPtr);
    auto *responses = static_cast<pam_response *>(calloc(static_cast<size_t>(numMsg), sizeof(pam_response)));
    if (!responses) {
        return PAM_BUF_ERR;
    }

    for (int i = 0; i < numMsg; ++i) {
        if (!msg[i]) {
            free(responses);
            return PAM_CONV_ERR;
        }

        switch (msg[i]->msg_style) {
        case PAM_PROMPT_ECHO_OFF:
        case PAM_PROMPT_ECHO_ON:
            responses[i].resp = strdup(data->password ? data->password : "");
            responses[i].resp_retcode = 0;
            break;
        case PAM_ERROR_MSG:
        case PAM_TEXT_INFO:
            responses[i].resp = nullptr;
            responses[i].resp_retcode = 0;
            break;
        default:
            for (int j = 0; j <= i; ++j) {
                if (responses[j].resp) {
                    free(responses[j].resp);
                }
            }
            free(responses);
            return PAM_CONV_ERR;
        }
    }

    *resp = responses;
    return PAM_SUCCESS;
}
} // namespace
#endif

bool SessionAuthenticator::authenticateCurrentUser(const QString &password, QString *reason)
{
    if (password.isEmpty()) {
        if (reason) {
            *reason = QStringLiteral("empty-password");
        }
        return false;
    }

#ifdef SLM_HAVE_PAM
    const QString user = QProcessEnvironment::systemEnvironment().value(QStringLiteral("USER")).trimmed();
    if (user.isEmpty()) {
        if (reason) {
            *reason = QStringLiteral("missing-user");
        }
        return false;
    }

    const QByteArray userUtf8 = user.toUtf8();
    const QByteArray passUtf8 = password.toUtf8();

    PamConversationData data;
    data.password = passUtf8.constData();

    pam_conv conv;
    conv.conv = pamConversation;
    conv.appdata_ptr = &data;

    pam_handle_t *pamh = nullptr;
    int rc = pam_start("login", userUtf8.constData(), &conv, &pamh);
    if (rc != PAM_SUCCESS || !pamh) {
        if (reason) {
            *reason = QStringLiteral("pam-start-failed");
        }
        if (pamh) {
            pam_end(pamh, rc);
        }
        return false;
    }

    rc = pam_authenticate(pamh, 0);
    if (rc == PAM_SUCCESS) {
        rc = pam_acct_mgmt(pamh, 0);
    }

    const bool ok = (rc == PAM_SUCCESS);
    if (!ok && reason) {
        *reason = QStringLiteral("pam-auth-failed");
    }

    pam_end(pamh, rc);
    return ok;
#else
    if (reason) {
        *reason = QStringLiteral("pam-unavailable");
    }
    return false;
#endif
}
