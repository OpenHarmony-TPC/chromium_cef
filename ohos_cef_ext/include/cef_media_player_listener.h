
#ifndef CEF_INCLUDE_CEF_MEDIA_PLAYER_LISTENER_H_
#define CEF_INCLUDE_CEF_MEDIA_PLAYER_LISTENER_H_

#include "build/build_config.h"
#include "cef/include/cef_base.h"

///
/// Class used to monitor custom media player status.
///
/*--cef(source=library)--*/
class CefMediaPlayerListener : public virtual CefBaseRefCounted {
 public:
  ///
  /// triggered on media duration changed
  ///
  /*--cef()--*/
  virtual ~CefMediaPlayerListener() = default;
  ///
  /// triggered on media duration changed
  ///
  /*--cef()--*/
  virtual void OnStatusChanged(uint32_t status) = 0;
  ///
  /// triggered on media duration changed
  ///
  /*--cef()--*/
  virtual void OnVolumeChanged(double volume) = 0;
  ///
  /// triggered on media duration changed
  ///
  /*--cef()--*/
  virtual void OnMutedChanged(bool muted) = 0;
  ///
  /// triggered on media duration changed
  ///
  /*--cef()--*/
  virtual void OnPlaybackRateChanged(double playback_rate) = 0;
  ///
  /// triggered on media duration changed
  ///
  /*--cef()--*/
  virtual void OnDurationChanged(double duration) = 0;
  ///
  /// triggered on media duration changed
  ///
  /*--cef()--*/
  virtual void OnTimeUpdate(double current_time) = 0;
  ///
  /// triggered on media duration changed
  ///
  /*--cef()--*/
  virtual void OnBufferedEndTimeChanged(double buffered_time) = 0;
  ///
  /// triggered on media duration changed
  ///
  /*--cef()--*/
  virtual void OnEnded() = 0;
  ///
  /// triggered on media duration changed
  ///
  /*--cef()--*/
  virtual void OnNetworkStateChanged(uint32_t state) = 0;
  ///
  /// triggered on media duration changed
  ///
  /*--cef()--*/
  virtual void OnReadyStateChanged(uint32_t state) = 0;
  ///
  /// triggered on media duration changed
  ///
  /*--cef()--*/
  virtual void OnFullscreenChanged(bool fullscreen) = 0;
  ///
  /// triggered on media duration changed
  ///
  /*--cef()--*/
  virtual void OnSeeking() = 0;
  ///
  /// triggered on media duration changed
  ///
  /*--cef()--*/
  virtual void OnSeekFinished() = 0;
  ///
  /// triggered on media duration changed
  ///
  /*--cef()--*/
  virtual void OnError(uint32_t error_code, const CefString& error_msg) = 0;
  ///
  /// triggered on media duration changed
  ///
  /*--cef()--*/
  virtual void OnVideoSizeChanged(int width, int height) = 0;
};

#endif  // CEF_INCLUDE_CEF_MEDIA_PLAYER_LISTENER_H_
