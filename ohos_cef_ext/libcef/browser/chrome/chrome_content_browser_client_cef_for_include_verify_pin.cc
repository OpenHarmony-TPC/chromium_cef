class CefVerifyPinCallbackImpl : public CefVerifyPinCallback {
 public:
  explicit CefVerifyPinCallbackImpl(
      std::unique_ptr<content::ClientCertificateDelegate> delegate,
      CefRefPtr<CefX509Certificate> cert,
      std::string identity,
      scoped_refptr<net::SSLPrivateKey> ssl_private_key)
      : delegate_(std::move(delegate)),
        cert_(cert),
        identity_(identity),
        ssl_private_key_(ssl_private_key) {}
 
  CefVerifyPinCallbackImpl(const CefVerifyPinCallbackImpl&) = delete;
  CefVerifyPinCallbackImpl& operator=(const CefVerifyPinCallbackImpl&) = delete;
 
  void Confirm(int32_t verifyResult) override {
    LOG(INFO) << "CefVerifyPinCallbackImpl::Confirm verifyResult: " << verifyResult;
    CEF_REQUIRE_UIT();
    if (!cert_) {
      LOG(ERROR) << "CefVerifyPinCallbackImpl::Confirm cert_ is nullptr";
      delegate_->ContinueWithCertificate(nullptr, nullptr);
      return;
    }
    if (!identity_.c_str()) {
      LOG(ERROR) << "CefVerifyPinCallbackImpl Confirm identity is empty";
      delegate_->ContinueWithCertificate(nullptr, nullptr);
      return;
    }
 
    if (verifyResult != PinVerificationResult::PIN_VERIFICATION_SUCCESS) {
      delegate_->ContinueWithCertificate(nullptr, nullptr);
      return;
    }
 
    if (ssl_private_key_) {
      CefX509CertificateImpl* certImpl = static_cast<CefX509CertificateImpl*>(cert_.get());
      delegate_->ContinueWithCertificate(certImpl->GetInternalCertObject(), ssl_private_key_);
    } else {
      delegate_->ContinueWithCertificate(nullptr, nullptr);
    }
  }
 
  [[nodiscard]] std::unique_ptr<content::ClientCertificateDelegate>
  DisconnectDelegate() {
    LOG(DEBUG) << "CefVerifyPinCallbackImpl::DisconnectDelegate";
    CEF_REQUIRE_UIT();
    return std::move(delegate_);
  }
 
 private:
  enum PinVerificationResult {
    PIN_VERIFICATION_SUCCESS = 0,
    PIN_VERIFICATION_FAILED = 1
  };
 
  std::unique_ptr<content::ClientCertificateDelegate> delegate_;
  CefRefPtr<CefX509Certificate> cert_;
  std::string identity_;
  scoped_refptr<net::SSLPrivateKey> ssl_private_key_;
 
  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefSelectClientCertificateCallbackImpl);
};