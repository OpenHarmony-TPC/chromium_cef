#include "libcef/browser/download_resume_util.h"
#include "base/guid.h"
#include "base/time/time.h"
#include "components/download/public/common/download_item_impl.h"

namespace download_resume_util {
const char kNWebId[] = "nweb_id";

void ResumeDownloadWithId(
    content::DownloadManager* manager,
    const std::string& guid,
    const GURL& gurl,
    const base::FilePath& full_path,
    int64 received_bytes,
    int64 total_bytes,
    const std::string& etag,
    const std::string& mime_type,
    const std::string& last_modified,
    std::vector<download::DownloadItem::ReceivedSlice> received_slices,
    uint32_t next_id) {
  std::vector<GURL> url_chain;
  url_chain.push_back(gurl);
  auto download_item = manager->CreateDownloadItem(
      guid,                                         /*guid*/
      next_id,                                      /*id*/
      full_path,                                    /*current_path*/
      full_path,                                    /*target_path*/
      url_chain,                                    /*url_chain*/
      GURL(),                                       /*referrer_url*/
      GURL(),                                       /*site_url*/
      GURL(),                                       /*tab_url*/
      GURL(),                                       /*tab_referrer_url*/
      url::Origin(),                                /*request_initiator*/
      mime_type,                                    /*mime_type*/
      mime_type,                                    /*original_mime_type*/
      base::Time::Now(),                            /*start_time*/
      base::Time::Now(),                            /*end_time*/
      etag,                                         /*etag*/
      last_modified,                                /*last_modified*/
      received_bytes,                               /*received_bytes*/
      total_bytes,                                  /*total_bytes*/
      std::string(),                                /*hash*/
      download::DownloadItem::INTERRUPTED,          /*state*/
      download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS, /*danger_type*/
      download::DOWNLOAD_INTERRUPT_REASON_NONE,     /*interrupt_reason*/
      false,                                        /*opened*/
      base::Time(),                                 /*last_access_time*/
      false,                                        /*transient*/
      received_slices,                              /*received_slices*/
      download::DownloadItemRerouteInfo()           /*reroute_info*/
  );
  if (download_item) {
    download_item->SetUserData(
        kNWebId, std::make_unique<download::DownloadItemImpl::NWebIdData>(-1));
    download_item->Resume(true /*is_user_resume*/);
  }
}
}  //  namespace download_resume_util
