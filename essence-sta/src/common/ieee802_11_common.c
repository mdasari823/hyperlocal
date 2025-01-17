/*
 * IEEE 802.11 Common routines
 * Copyright (c) 2002-2013, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "defs.h"
#include "ieee802_11_defs.h"
#include "ieee802_11_common.h"


static int ieee802_11_parse_vendor_specific(const u8 *pos, size_t elen,
					    struct ieee802_11_elems *elems,
					    int show_errors)
{
	unsigned int oui;

	/* first 3 bytes in vendor specific information element are the IEEE
	 * OUI of the vendor. The following byte is used a vendor specific
	 * sub-type. */
	if (elen < 4) {
		if (show_errors) {
			wpa_printf(MSG_MSGDUMP, "short vendor specific "
				   "information element ignored (len=%lu)",
				   (unsigned long) elen);
		}
		return -1;
	}

	oui = WPA_GET_BE24(pos);
	switch (oui) {
	case OUI_MICROSOFT:
		/* Microsoft/Wi-Fi information elements are further typed and
		 * subtyped */
		switch (pos[3]) {
		case 1:
			/* Microsoft OUI (00:50:F2) with OUI Type 1:
			 * real WPA information element */
			elems->wpa_ie = pos;
			elems->wpa_ie_len = elen;
			break;
		case WMM_OUI_TYPE:
			/* WMM information element */
			if (elen < 5) {
				wpa_printf(MSG_MSGDUMP, "short WMM "
					   "information element ignored "
					   "(len=%lu)",
					   (unsigned long) elen);
				return -1;
			}
			switch (pos[4]) {
			case WMM_OUI_SUBTYPE_INFORMATION_ELEMENT:
			case WMM_OUI_SUBTYPE_PARAMETER_ELEMENT:
				/*
				 * Share same pointer since only one of these
				 * is used and they start with same data.
				 * Length field can be used to distinguish the
				 * IEs.
				 */
				elems->wmm = pos;
				elems->wmm_len = elen;
				break;
			case WMM_OUI_SUBTYPE_TSPEC_ELEMENT:
				elems->wmm_tspec = pos;
				elems->wmm_tspec_len = elen;
				break;
			default:
				wpa_printf(MSG_EXCESSIVE, "unknown WMM "
					   "information element ignored "
					   "(subtype=%d len=%lu)",
					   pos[4], (unsigned long) elen);
				return -1;
			}
			break;
		case 4:
			/* Wi-Fi Protected Setup (WPS) IE */
			elems->wps_ie = pos;
			elems->wps_ie_len = elen;
			break;
		default:
			wpa_printf(MSG_EXCESSIVE, "Unknown Microsoft "
				   "information element ignored "
				   "(type=%d len=%lu)",
				   pos[3], (unsigned long) elen);
			return -1;
		}
		break;

	case OUI_WFA:
		switch (pos[3]) {
		case P2P_OUI_TYPE:
			/* Wi-Fi Alliance - P2P IE */
			elems->p2p = pos;
			elems->p2p_len = elen;
			break;
		case WFD_OUI_TYPE:
			/* Wi-Fi Alliance - WFD IE */
			elems->wfd = pos;
			elems->wfd_len = elen;
			break;
		case HS20_INDICATION_OUI_TYPE:
			/* Hotspot 2.0 */
			elems->hs20 = pos;
			elems->hs20_len = elen;
			break;
		case HS20_OSEN_OUI_TYPE:
			/* Hotspot 2.0 OSEN */
			elems->osen = pos;
			elems->osen_len = elen;
			break;
		default:
			wpa_printf(MSG_MSGDUMP, "Unknown WFA "
				   "information element ignored "
				   "(type=%d len=%lu)",
				   pos[3], (unsigned long) elen);
			return -1;
		}
		break;

	case OUI_BROADCOM:
		switch (pos[3]) {
		case VENDOR_HT_CAPAB_OUI_TYPE:
			elems->vendor_ht_cap = pos;
			elems->vendor_ht_cap_len = elen;
			break;
		case VENDOR_VHT_TYPE:
			if (elen > 4 &&
			    (pos[4] == VENDOR_VHT_SUBTYPE ||
			     pos[4] == VENDOR_VHT_SUBTYPE2)) {
				elems->vendor_vht = pos;
				elems->vendor_vht_len = elen;
			} else
				return -1;
			break;
		default:
			wpa_printf(MSG_EXCESSIVE, "Unknown Broadcom "
				   "information element ignored "
				   "(type=%d len=%lu)",
				   pos[3], (unsigned long) elen);
			return -1;
		}
		break;

	default:
		wpa_printf(MSG_EXCESSIVE, "unknown vendor specific "
			   "information element ignored (vendor OUI "
			   "%02x:%02x:%02x len=%lu)",
			   pos[0], pos[1], pos[2], (unsigned long) elen);
		return -1;
	}

	return 0;
}


/**
 * ieee802_11_parse_elems - Parse information elements in management frames
 * @start: Pointer to the start of IEs
 * @len: Length of IE buffer in octets
 * @elems: Data structure for parsed elements
 * @show_errors: Whether to show parsing errors in debug log
 * Returns: Parsing result
 */
ParseRes ieee802_11_parse_elems(const u8 *start, size_t len,
				struct ieee802_11_elems *elems,
				int show_errors)
{
	size_t left = len;
	const u8 *pos = start;
	int unknown = 0;

	os_memset(elems, 0, sizeof(*elems));

	while (left >= 2) {
		u8 id, elen;

		id = *pos++;
		elen = *pos++;
		left -= 2;

		if (elen > left) {
			if (show_errors) {
				wpa_printf(MSG_DEBUG, "IEEE 802.11 element "
					   "parse failed (id=%d elen=%d "
					   "left=%lu)",
					   id, elen, (unsigned long) left);
				wpa_hexdump(MSG_MSGDUMP, "IEs", start, len);
			}
			return ParseFailed;
		}

		switch (id) {
		case WLAN_EID_SSID:
			elems->ssid = pos;
			elems->ssid_len = elen;
			break;
		case WLAN_EID_SUPP_RATES:
			elems->supp_rates = pos;
			elems->supp_rates_len = elen;
			break;
		case WLAN_EID_DS_PARAMS:
			elems->ds_params = pos;
			elems->ds_params_len = elen;
			break;
		case WLAN_EID_CF_PARAMS:
		case WLAN_EID_TIM:
			break;
		case WLAN_EID_CHALLENGE:
			elems->challenge = pos;
			elems->challenge_len = elen;
			break;
		case WLAN_EID_ERP_INFO:
			elems->erp_info = pos;
			elems->erp_info_len = elen;
			break;
		case WLAN_EID_EXT_SUPP_RATES:
			elems->ext_supp_rates = pos;
			elems->ext_supp_rates_len = elen;
			break;
		case WLAN_EID_VENDOR_SPECIFIC:
			if (ieee802_11_parse_vendor_specific(pos, elen,
							     elems,
							     show_errors))
				unknown++;
			break;
		case WLAN_EID_RSN:
			elems->rsn_ie = pos;
			elems->rsn_ie_len = elen;
			break;
		case WLAN_EID_PWR_CAPABILITY:
			break;
		case WLAN_EID_SUPPORTED_CHANNELS:
			elems->supp_channels = pos;
			elems->supp_channels_len = elen;
			break;
		case WLAN_EID_MOBILITY_DOMAIN:
			elems->mdie = pos;
			elems->mdie_len = elen;
			break;
		case WLAN_EID_FAST_BSS_TRANSITION:
			elems->ftie = pos;
			elems->ftie_len = elen;
			break;
		case WLAN_EID_TIMEOUT_INTERVAL:
			elems->timeout_int = pos;
			elems->timeout_int_len = elen;
			break;
		case WLAN_EID_HT_CAP:
			elems->ht_capabilities = pos;
			elems->ht_capabilities_len = elen;
			break;
		case WLAN_EID_HT_OPERATION:
			elems->ht_operation = pos;
			elems->ht_operation_len = elen;
			break;
		case WLAN_EID_MESH_CONFIG:
			elems->mesh_config = pos;
			elems->mesh_config_len = elen;
			break;
		case WLAN_EID_MESH_ID:
			elems->mesh_id = pos;
			elems->mesh_id_len = elen;
			break;
		case WLAN_EID_PEER_MGMT:
			elems->peer_mgmt = pos;
			elems->peer_mgmt_len = elen;
			break;
		case WLAN_EID_VHT_CAP:
			elems->vht_capabilities = pos;
			elems->vht_capabilities_len = elen;
			break;
		case WLAN_EID_VHT_OPERATION:
			elems->vht_operation = pos;
			elems->vht_operation_len = elen;
			break;
		case WLAN_EID_VHT_OPERATING_MODE_NOTIFICATION:
			if (elen != 1)
				break;
			elems->vht_opmode_notif = pos;
			break;
		case WLAN_EID_LINK_ID:
			if (elen < 18)
				break;
			elems->link_id = pos;
			break;
		case WLAN_EID_INTERWORKING:
			elems->interworking = pos;
			elems->interworking_len = elen;
			break;
		case WLAN_EID_QOS_MAP_SET:
			if (elen < 16)
				break;
			elems->qos_map_set = pos;
			elems->qos_map_set_len = elen;
			break;
		case WLAN_EID_EXT_CAPAB:
			elems->ext_capab = pos;
			elems->ext_capab_len = elen;
			break;
		case WLAN_EID_BSS_MAX_IDLE_PERIOD:
			if (elen < 3)
				break;
			elems->bss_max_idle_period = pos;
			break;
		case WLAN_EID_SSID_LIST:
			elems->ssid_list = pos;
			elems->ssid_list_len = elen;
			break;
		//CONFIG_ACTION_NOTIFICATION
		case WLAN_EID_NOT_INDICATOR:
			elems->afn = pos;
			elems->afn_len = elen;
			break;
		case WLAN_EID_AMPE:
			elems->ampe = pos;
			elems->ampe_len = elen;
			break;
		case WLAN_EID_MIC:
			elems->mic = pos;
			elems->mic_len = elen;
			/* after mic everything is encrypted, so stop. */
			left = elen;
			break;
		default:
			unknown++;
			if (!show_errors)
				break;
			wpa_printf(MSG_MSGDUMP, "IEEE 802.11 element parse "
				   "ignored unknown element (id=%d elen=%d)",
				   id, elen);
			break;
		}

		left -= elen;
		pos += elen;
	}

	if (left)
		return ParseFailed;

	return unknown ? ParseUnknown : ParseOK;
}


int ieee802_11_ie_count(const u8 *ies, size_t ies_len)
{
	int count = 0;
	const u8 *pos, *end;

	if (ies == NULL)
		return 0;

	pos = ies;
	end = ies + ies_len;

	while (pos + 2 <= end) {
		if (pos + 2 + pos[1] > end)
			break;
		count++;
		pos += 2 + pos[1];
	}

	return count;
}


struct wpabuf * ieee802_11_vendor_ie_concat(const u8 *ies, size_t ies_len,
					    u32 oui_type)
{
	struct wpabuf *buf;
	const u8 *end, *pos, *ie;

	pos = ies;
	end = ies + ies_len;
	ie = NULL;

	while (pos + 1 < end) {
		if (pos + 2 + pos[1] > end)
			return NULL;
		if (pos[0] == WLAN_EID_VENDOR_SPECIFIC && pos[1] >= 4 &&
		    WPA_GET_BE32(&pos[2]) == oui_type) {
			ie = pos;
			break;
		}
		pos += 2 + pos[1];
	}

	if (ie == NULL)
		return NULL; /* No specified vendor IE found */

	buf = wpabuf_alloc(ies_len);
	if (buf == NULL)
		return NULL;

	/*
	 * There may be multiple vendor IEs in the message, so need to
	 * concatenate their data fields.
	 */
	while (pos + 1 < end) {
		if (pos + 2 + pos[1] > end)
			break;
		if (pos[0] == WLAN_EID_VENDOR_SPECIFIC && pos[1] >= 4 &&
		    WPA_GET_BE32(&pos[2]) == oui_type)
			wpabuf_put_data(buf, pos + 6, pos[1] - 4);
		pos += 2 + pos[1];
	}

	return buf;
}


const u8 * get_hdr_bssid(const struct ieee80211_hdr *hdr, size_t len)
{
	u16 fc, type, stype;

	/*
	 * PS-Poll frames are 16 bytes. All other frames are
	 * 24 bytes or longer.
	 */
	if (len < 16)
		return NULL;

	fc = le_to_host16(hdr->frame_control);
	type = WLAN_FC_GET_TYPE(fc);
	stype = WLAN_FC_GET_STYPE(fc);

	switch (type) {
	case WLAN_FC_TYPE_DATA:
		if (len < 24)
			return NULL;
		switch (fc & (WLAN_FC_FROMDS | WLAN_FC_TODS)) {
		case WLAN_FC_FROMDS | WLAN_FC_TODS:
		case WLAN_FC_TODS:
			return hdr->addr1;
		case WLAN_FC_FROMDS:
			return hdr->addr2;
		default:
			return NULL;
		}
	case WLAN_FC_TYPE_CTRL:
		if (stype != WLAN_FC_STYPE_PSPOLL)
			return NULL;
		return hdr->addr1;
	case WLAN_FC_TYPE_MGMT:
		return hdr->addr3;
	default:
		return NULL;
	}
}


int hostapd_config_wmm_ac(struct hostapd_wmm_ac_params wmm_ac_params[],
			  const char *name, const char *val)
{
	int num, v;
	const char *pos;
	struct hostapd_wmm_ac_params *ac;

	/* skip 'wme_ac_' or 'wmm_ac_' prefix */
	pos = name + 7;
	if (os_strncmp(pos, "be_", 3) == 0) {
		num = 0;
		pos += 3;
	} else if (os_strncmp(pos, "bk_", 3) == 0) {
		num = 1;
		pos += 3;
	} else if (os_strncmp(pos, "vi_", 3) == 0) {
		num = 2;
		pos += 3;
	} else if (os_strncmp(pos, "vo_", 3) == 0) {
		num = 3;
		pos += 3;
	} else {
		wpa_printf(MSG_ERROR, "Unknown WMM name '%s'", pos);
		return -1;
	}

	ac = &wmm_ac_params[num];

	if (os_strcmp(pos, "aifs") == 0) {
		v = atoi(val);
		if (v < 1 || v > 255) {
			wpa_printf(MSG_ERROR, "Invalid AIFS value %d", v);
			return -1;
		}
		ac->aifs = v;
	} else if (os_strcmp(pos, "cwmin") == 0) {
		v = atoi(val);
		if (v < 0 || v > 12) {
			wpa_printf(MSG_ERROR, "Invalid cwMin value %d", v);
			return -1;
		}
		ac->cwmin = v;
	} else if (os_strcmp(pos, "cwmax") == 0) {
		v = atoi(val);
		if (v < 0 || v > 12) {
			wpa_printf(MSG_ERROR, "Invalid cwMax value %d", v);
			return -1;
		}
		ac->cwmax = v;
	} else if (os_strcmp(pos, "txop_limit") == 0) {
		v = atoi(val);
		if (v < 0 || v > 0xffff) {
			wpa_printf(MSG_ERROR, "Invalid txop value %d", v);
			return -1;
		}
		ac->txop_limit = v;
	} else if (os_strcmp(pos, "acm") == 0) {
		v = atoi(val);
		if (v < 0 || v > 1) {
			wpa_printf(MSG_ERROR, "Invalid acm value %d", v);
			return -1;
		}
		ac->admission_control_mandatory = v;
	} else {
		wpa_printf(MSG_ERROR, "Unknown wmm_ac_ field '%s'", pos);
		return -1;
	}

	return 0;
}


enum hostapd_hw_mode ieee80211_freq_to_chan(int freq, u8 *channel)
{
	enum hostapd_hw_mode mode = NUM_HOSTAPD_MODES;

	if (freq >= 2412 && freq <= 2472) {
		mode = HOSTAPD_MODE_IEEE80211G;
		*channel = (freq - 2407) / 5;
	} else if (freq == 2484) {
		mode = HOSTAPD_MODE_IEEE80211B;
		*channel = 14;
	} else if (freq >= 4900 && freq < 5000) {
		mode = HOSTAPD_MODE_IEEE80211A;
		*channel = (freq - 4000) / 5;
	} else if (freq >= 5000 && freq < 5900) {
		mode = HOSTAPD_MODE_IEEE80211A;
		*channel = (freq - 5000) / 5;
	} else if (freq >= 56160 + 2160 * 1 && freq <= 56160 + 2160 * 4) {
		mode = HOSTAPD_MODE_IEEE80211AD;
		*channel = (freq - 56160) / 2160;
	}

	return mode;
}


static const char *us_op_class_cc[] = {
	"US", "CA", NULL
};

static const char *eu_op_class_cc[] = {
	"AL", "AM", "AT", "AZ", "BA", "BE", "BG", "BY", "CH", "CY", "CZ", "DE",
	"DK", "EE", "EL", "ES", "FI", "FR", "GE", "HR", "HU", "IE", "IS", "IT",
	"LI", "LT", "LU", "LV", "MD", "ME", "MK", "MT", "NL", "NO", "PL", "PT",
	"RO", "RS", "RU", "SE", "SI", "SK", "TR", "UA", "UK", NULL
};

static const char *jp_op_class_cc[] = {
	"JP", NULL
};

static const char *cn_op_class_cc[] = {
	"CN", "CA", NULL
};


static int country_match(const char *cc[], const char *country)
{
	int i;

	if (country == NULL)
		return 0;
	for (i = 0; cc[i]; i++) {
		if (cc[i][0] == country[0] && cc[i][1] == country[1])
			return 1;
	}

	return 0;
}


static int ieee80211_chan_to_freq_us(u8 op_class, u8 chan)
{
	switch (op_class) {
	case 12: /* channels 1..11 */
	case 32: /* channels 1..7; 40 MHz */
	case 33: /* channels 5..11; 40 MHz */
		if (chan < 1 || chan > 11)
			return -1;
		return 2407 + 5 * chan;
	case 1: /* channels 36,40,44,48 */
	case 2: /* channels 52,56,60,64; dfs */
	case 22: /* channels 36,44; 40 MHz */
	case 23: /* channels 52,60; 40 MHz */
	case 27: /* channels 40,48; 40 MHz */
	case 28: /* channels 56,64; 40 MHz */
		if (chan < 36 || chan > 64)
			return -1;
		return 5000 + 5 * chan;
	case 4: /* channels 100-144 */
	case 24: /* channels 100-140; 40 MHz */
		if (chan < 100 || chan > 144)
			return -1;
		return 5000 + 5 * chan;
	case 3: /* channels 149,153,157,161 */
	case 25: /* channels 149,157; 40 MHz */
	case 26: /* channels 149,157; 40 MHz */
	case 30: /* channels 153,161; 40 MHz */
	case 31: /* channels 153,161; 40 MHz */
		if (chan < 149 || chan > 161)
			return -1;
		return 5000 + 5 * chan;
	case 34: /* 60 GHz band, channels 1..3 */
		if (chan < 1 || chan > 3)
			return -1;
		return 56160 + 2160 * chan;
	}
	return -1;
}


static int ieee80211_chan_to_freq_eu(u8 op_class, u8 chan)
{
	switch (op_class) {
	case 4: /* channels 1..13 */
	case 11: /* channels 1..9; 40 MHz */
	case 12: /* channels 5..13; 40 MHz */
		if (chan < 1 || chan > 13)
			return -1;
		return 2407 + 5 * chan;
	case 1: /* channels 36,40,44,48 */
	case 2: /* channels 52,56,60,64; dfs */
	case 5: /* channels 36,44; 40 MHz */
	case 6: /* channels 52,60; 40 MHz */
	case 8: /* channels 40,48; 40 MHz */
	case 9: /* channels 56,64; 40 MHz */
		if (chan < 36 || chan > 64)
			return -1;
		return 5000 + 5 * chan;
	case 3: /* channels 100-140 */
	case 7: /* channels 100-132; 40 MHz */
	case 10: /* channels 104-136; 40 MHz */
	case 16: /* channels 100-140 */
		if (chan < 100 || chan > 140)
			return -1;
		return 5000 + 5 * chan;
	case 17: /* channels 149,153,157,161,165,169 */
		if (chan < 149 || chan > 169)
			return -1;
		return 5000 + 5 * chan;
	case 18: /* 60 GHz band, channels 1..4 */
		if (chan < 1 || chan > 4)
			return -1;
		return 56160 + 2160 * chan;
	}
	return -1;
}


static int ieee80211_chan_to_freq_jp(u8 op_class, u8 chan)
{
	switch (op_class) {
	case 30: /* channels 1..13 */
	case 56: /* channels 1..9; 40 MHz */
	case 57: /* channels 5..13; 40 MHz */
		if (chan < 1 || chan > 13)
			return -1;
		return 2407 + 5 * chan;
	case 31: /* channel 14 */
		if (chan != 14)
			return -1;
		return 2414 + 5 * chan;
	case 1: /* channels 34,38,42,46(old) or 36,40,44,48 */
	case 32: /* channels 52,56,60,64 */
	case 33: /* channels 52,56,60,64 */
	case 36: /* channels 36,44; 40 MHz */
	case 37: /* channels 52,60; 40 MHz */
	case 38: /* channels 52,60; 40 MHz */
	case 41: /* channels 40,48; 40 MHz */
	case 42: /* channels 56,64; 40 MHz */
	case 43: /* channels 56,64; 40 MHz */
		if (chan < 34 || chan > 64)
			return -1;
		return 5000 + 5 * chan;
	case 34: /* channels 100-140 */
	case 35: /* channels 100-140 */
	case 39: /* channels 100-132; 40 MHz */
	case 40: /* channels 100-132; 40 MHz */
	case 44: /* channels 104-136; 40 MHz */
	case 45: /* channels 104-136; 40 MHz */
	case 58: /* channels 100-140 */
		if (chan < 100 || chan > 140)
			return -1;
		return 5000 + 5 * chan;
	case 59: /* 60 GHz band, channels 1..4 */
		if (chan < 1 || chan > 3)
			return -1;
		return 56160 + 2160 * chan;
	}
	return -1;
}


static int ieee80211_chan_to_freq_cn(u8 op_class, u8 chan)
{
	switch (op_class) {
	case 7: /* channels 1..13 */
	case 8: /* channels 1..9; 40 MHz */
	case 9: /* channels 5..13; 40 MHz */
		if (chan < 1 || chan > 13)
			return -1;
		return 2407 + 5 * chan;
	case 1: /* channels 36,40,44,48 */
	case 2: /* channels 52,56,60,64; dfs */
	case 4: /* channels 36,44; 40 MHz */
	case 5: /* channels 52,60; 40 MHz */
		if (chan < 36 || chan > 64)
			return -1;
		return 5000 + 5 * chan;
	case 3: /* channels 149,153,157,161,165 */
	case 6: /* channels 149,157; 40 MHz */
		if (chan < 149 || chan > 165)
			return -1;
		return 5000 + 5 * chan;
	}
	return -1;
}


static int ieee80211_chan_to_freq_global(u8 op_class, u8 chan)
{
	/* Table E-4 in IEEE Std 802.11-2012 - Global operating classes */
	switch (op_class) {
	case 81:
		/* channels 1..13 */
		if (chan < 1 || chan > 13)
			return -1;
		return 2407 + 5 * chan;
	case 82:
		/* channel 14 */
		if (chan != 14)
			return -1;
		return 2414 + 5 * chan;
	case 83: /* channels 1..9; 40 MHz */
	case 84: /* channels 5..13; 40 MHz */
		if (chan < 1 || chan > 13)
			return -1;
		return 2407 + 5 * chan;
	case 115: /* channels 36,40,44,48; indoor only */
	case 116: /* channels 36,44; 40 MHz; indoor only */
	case 117: /* channels 40,48; 40 MHz; indoor only */
	case 118: /* channels 52,56,60,64; dfs */
	case 119: /* channels 52,60; 40 MHz; dfs */
	case 120: /* channels 56,64; 40 MHz; dfs */
		if (chan < 36 || chan > 64)
			return -1;
		return 5000 + 5 * chan;
	case 121: /* channels 100-140 */
	case 122: /* channels 100-142; 40 MHz */
	case 123: /* channels 104-136; 40 MHz */
		if (chan < 100 || chan > 140)
			return -1;
		return 5000 + 5 * chan;
	case 124: /* channels 149,153,157,161 */
	case 125: /* channels 149,153,157,161,165,169 */
	case 126: /* channels 149,157; 40 MHz */
	case 127: /* channels 153,161; 40 MHz */
		if (chan < 149 || chan > 161)
			return -1;
		return 5000 + 5 * chan;
	case 128: /* center freqs 42, 58, 106, 122, 138, 155; 80 MHz */
	case 130: /* center freqs 42, 58, 106, 122, 138, 155; 80 MHz */
		if (chan < 36 || chan > 161)
			return -1;
		return 5000 + 5 * chan;
	case 129: /* center freqs 50, 114; 160 MHz */
		if (chan < 50 || chan > 114)
			return -1;
		return 5000 + 5 * chan;
	case 180: /* 60 GHz band, channels 1..4 */
		if (chan < 1 || chan > 4)
			return -1;
		return 56160 + 2160 * chan;
	}
	return -1;
}

/**
 * ieee80211_chan_to_freq - Convert channel info to frequency
 * @country: Country code, if known; otherwise, global operating class is used
 * @op_class: Operating class
 * @chan: Channel number
 * Returns: Frequency in MHz or -1 if the specified channel is unknown
 */
int ieee80211_chan_to_freq(const char *country, u8 op_class, u8 chan)
{
	int freq;

	if (country_match(us_op_class_cc, country)) {
		freq = ieee80211_chan_to_freq_us(op_class, chan);
		if (freq > 0)
			return freq;
	}

	if (country_match(eu_op_class_cc, country)) {
		freq = ieee80211_chan_to_freq_eu(op_class, chan);
		if (freq > 0)
			return freq;
	}

	if (country_match(jp_op_class_cc, country)) {
		freq = ieee80211_chan_to_freq_jp(op_class, chan);
		if (freq > 0)
			return freq;
	}

	if (country_match(cn_op_class_cc, country)) {
		freq = ieee80211_chan_to_freq_cn(op_class, chan);
		if (freq > 0)
			return freq;
	}

	return ieee80211_chan_to_freq_global(op_class, chan);
}


int ieee80211_is_dfs(int freq)
{
	/* TODO: this could be more accurate to better cover all domains */
	return (freq >= 5260 && freq <= 5320) || (freq >= 5500 && freq <= 5700);
}


static int is_11b(u8 rate)
{
	return rate == 0x02 || rate == 0x04 || rate == 0x0b || rate == 0x16;
}


int supp_rates_11b_only(struct ieee802_11_elems *elems)
{
	int num_11b = 0, num_others = 0;
	int i;

	if (elems->supp_rates == NULL && elems->ext_supp_rates == NULL)
		return 0;

	for (i = 0; elems->supp_rates && i < elems->supp_rates_len; i++) {
		if (is_11b(elems->supp_rates[i]))
			num_11b++;
		else
			num_others++;
	}

	for (i = 0; elems->ext_supp_rates && i < elems->ext_supp_rates_len;
	     i++) {
		if (is_11b(elems->ext_supp_rates[i]))
			num_11b++;
		else
			num_others++;
	}

	return num_11b > 0 && num_others == 0;
}


const char * fc2str(u16 fc)
{
	u16 stype = WLAN_FC_GET_STYPE(fc);
#define C2S(x) case x: return #x;

	switch (WLAN_FC_GET_TYPE(fc)) {
	case WLAN_FC_TYPE_MGMT:
		switch (stype) {
		C2S(WLAN_FC_STYPE_ASSOC_REQ)
		C2S(WLAN_FC_STYPE_ASSOC_RESP)
		C2S(WLAN_FC_STYPE_REASSOC_REQ)
		C2S(WLAN_FC_STYPE_REASSOC_RESP)
		C2S(WLAN_FC_STYPE_PROBE_REQ)
		C2S(WLAN_FC_STYPE_PROBE_RESP)
		C2S(WLAN_FC_STYPE_BEACON)
		C2S(WLAN_FC_STYPE_ATIM)
		C2S(WLAN_FC_STYPE_DISASSOC)
		C2S(WLAN_FC_STYPE_AUTH)
		C2S(WLAN_FC_STYPE_DEAUTH)
		C2S(WLAN_FC_STYPE_ACTION)
		}
		break;
	case WLAN_FC_TYPE_CTRL:
		switch (stype) {
		C2S(WLAN_FC_STYPE_PSPOLL)
		C2S(WLAN_FC_STYPE_RTS)
		C2S(WLAN_FC_STYPE_CTS)
		C2S(WLAN_FC_STYPE_ACK)
		C2S(WLAN_FC_STYPE_CFEND)
		C2S(WLAN_FC_STYPE_CFENDACK)
		}
		break;
	case WLAN_FC_TYPE_DATA:
		switch (stype) {
		C2S(WLAN_FC_STYPE_DATA)
		C2S(WLAN_FC_STYPE_DATA_CFACK)
		C2S(WLAN_FC_STYPE_DATA_CFPOLL)
		C2S(WLAN_FC_STYPE_DATA_CFACKPOLL)
		C2S(WLAN_FC_STYPE_NULLFUNC)
		C2S(WLAN_FC_STYPE_CFACK)
		C2S(WLAN_FC_STYPE_CFPOLL)
		C2S(WLAN_FC_STYPE_CFACKPOLL)
		C2S(WLAN_FC_STYPE_QOS_DATA)
		C2S(WLAN_FC_STYPE_QOS_DATA_CFACK)
		C2S(WLAN_FC_STYPE_QOS_DATA_CFPOLL)
		C2S(WLAN_FC_STYPE_QOS_DATA_CFACKPOLL)
		C2S(WLAN_FC_STYPE_QOS_NULL)
		C2S(WLAN_FC_STYPE_QOS_CFPOLL)
		C2S(WLAN_FC_STYPE_QOS_CFACKPOLL)
		}
		break;
	}
	return "WLAN_FC_TYPE_UNKNOWN";
#undef C2S
}
