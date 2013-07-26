/* openrtb_bid_request.cc
   Jeremy Barnes, 19 February 2013
   Copyright (c) 2013 Datacratic Inc.  All rights reserved.

   Bid request parser for OpenRTB.
*/

#include "openrtb_bid_request.h"
#include "jml/utils/json_parsing.h"
#include "openrtb/openrtb.h"
#include "openrtb/openrtb_parsing.h"

using namespace std;

namespace RTBKIT {


/*****************************************************************************/
/* OPENRTB BID REQUEST PARSER                                                */
/*****************************************************************************/

BidRequest *
fromOpenRtb(OpenRTB::BidRequest && req,
            const std::string & provider,
            const std::string & exchange)
{
    std::unique_ptr<BidRequest> result(new BidRequest());

    result->auctionId = std::move(req.id);
    result->auctionType = AuctionType::SECOND_PRICE;
    if (req.tmax)
        result->timeAvailableMs = *req.tmax;
    result->timestamp = Date::now();
    result->isTest = false;
    result->unparseable = std::move(req.unparseable);

    result->provider = provider;
    result->exchange = (exchange.empty() ? provider : exchange);

    result->imp.reserve(req.imp.size());

    for (auto & i: req.imp)
    {
        AdSpot spot(std::move(i));

        // Copy the ad formats in for the moment
        if (spot.banner)
            for (unsigned i = 0; i < spot.banner->w.size(); ++i)
                spot.formats.push_back(Format(spot.banner->w[i], spot.banner->h[i]));

        result->imp.emplace_back(std::move(spot));
    }

    if (req.site && req.app)
        throw ML::Exception("can't have site and app");

    if (req.site) {
        result->site = req.site;
        if (!result->site->page.empty())
            result->url = result->site->page;
        else if (result->site->id)
            result->url = Url("http://" + result->site->id.toString() + ".siteid/");
    }
    else if (req.app) {
        result->app = req.app;

        if (!result->app->bundle.empty())
            result->url = Url(result->app->bundle);
        else if (result->app->id)
            result->url = Url("http://" + result->app->id.toString() + ".appid/");
    }

    if (req.device) {
        result->device = req.device;
        result->language = result->device->language;
        result->userAgent = result->device->ua;
        if (!result->device->ip.empty())
            result->ipAddress = result->device->ip;
        else if (!result->device->ipv6.empty())
            result->ipAddress = result->device->ipv6;

        if (result->device->geo) {
            const auto & g = *result->device->geo;
            auto & l = result->location;
            l.countryCode = g.country;
            if (!g.region.empty())
                l.regionCode = g.region;
            else l.regionCode = g.regionfips104;
            l.cityName = g.city;
            l.postalCode = g.zip;
            
            // TODO: DMA
        }
    }

    if (req.user) {
        result->user = req.user;
        for (auto & d: result->user->data) {
            string key;
            if (d.id)
                key = d.id.toString();
            else key = d.name;

            vector<string> values;
            for (auto & v: d.segment) {
                if (v.id)
                    values.push_back(v.id.toString());
                else if (!v.name.empty())
                    values.push_back(v.name);
            }

            result->segments.addStrings(key, values);
        }

        if (result->user->ext.isMember("tz"))
            result->location.timezoneOffsetMinutes = result->user->ext["tz"].asInt();

        if (result->user->id)
            result->userIds.add(result->user->id, ID_EXCHANGE);
        if (result->user->buyeruid)
            result->userIds.add(result->user->buyeruid, ID_PROVIDER);
    }

    if (req.cur)
        for (const auto& cur : *req.cur)
            result->bidCurrency.emplace_back(CurrencyCode(uint32_t(cur)));
    else // exchange-specific currency
        result->bidCurrency.push_back(CurrencyCode::CC_USD);

    result->ext = std::move(req.ext);
    
    return result.release();
}

namespace {

static DefaultDescription<OpenRTB::BidRequest> desc;

} // file scope

BidRequest *
OpenRtbBidRequestParser::
parseBidRequest(const std::string & jsonValue,
                const std::string & provider,
                const std::string & exchange)
{
    return fromOpenRtb(RTBKIT::fromJson<OpenRTB::BidRequest>(jsonValue), provider, exchange);
}

BidRequest *
OpenRtbBidRequestParser::
parseBidRequest(ML::Parse_Context & context,
                const std::string & provider,
                const std::string & exchange)
{
    return fromOpenRtb(RTBKIT::fromJson<OpenRTB::BidRequest>(context), provider, exchange);
}

} // namespace RTBKIT
