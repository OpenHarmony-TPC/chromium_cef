#ifndef CEF_LIBCEF_BROWSER_ALLOY_RENDER_PROCESS_STATE_HANDLER_H_
#define CEF_LIBCEF_BROWSER_ALLOY_RENDER_PROCESS_STATE_HANDLER_H_

#include <vector>

struct WebComponentState
{
  int nweb_id;
  bool state;
};

struct RenderProcessStateMap
{
  uint32_t render_process_id;
  std::vector<WebComponentState> web_component_list;
};

class RenderProcessStateHandler
{
private:
  RenderProcessStateHandler();
  ~RenderProcessStateHandler();

  RenderProcessStateHandler(const RenderProcessStateHandler &) = delete;
  RenderProcessStateHandler &operator=(const RenderProcessStateHandler &) = delete;

  std::vector<RenderProcessStateMap> render_process_map_list_;

  std::vector<WebComponentState> initial_web_component_list_;

  static RenderProcessStateHandler *instance;

public:
  static RenderProcessStateHandler *GetInstance();

  void UpdateRenderProcessState(
      uint32_t render_process_id,
      int nweb_id,
      bool is_to_background);

  void InitRenderProcessState(
      uint32_t render_process_id,
      int nweb_id);
};
