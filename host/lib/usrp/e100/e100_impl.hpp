//
// Copyright 2010-2011 Ettus Research LLC
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include "e100_iface.hpp"
#include "clock_ctrl.hpp"
#include "codec_ctrl.hpp"
#include <uhd/device.hpp>
#include <uhd/utils/pimpl.hpp>
#include <uhd/usrp/subdev_spec.hpp>
#include <uhd/usrp/dboard_eeprom.hpp>
#include <uhd/types/otw_type.hpp>
#include <uhd/types/clock_config.hpp>
#include <uhd/types/stream_cmd.hpp>
#include <uhd/usrp/dboard_manager.hpp>
#include <uhd/transport/zero_copy.hpp>

#ifndef INCLUDED_E100_IMPL_HPP
#define INCLUDED_E100_IMPL_HPP

uhd::transport::zero_copy_if::sptr e100_make_mmap_zero_copy(e100_iface::sptr iface);

static const std::string     E100_FPGA_FILE_NAME = "usrp_e100_fpga_v2.bin";
static const boost::uint16_t E100_FPGA_COMPAT_NUM = 0x05;
static const double          E100_DEFAULT_CLOCK_RATE = 64e6;
static const size_t          E100_NUM_RX_DSPS = 2;
static const size_t          E100_NUM_TX_DSPS = 1;
static const boost::uint32_t E100_DSP_SID_BASE = 2; //leave room for other dsp (increments by 1)
static const boost::uint32_t E100_ASYNC_SID = 1;

//! load an fpga image from a bin file into the usrp-e fpga
extern void e100_load_fpga(const std::string &bin_file);

/*!
 * Make a usrp-e dboard interface.
 * \param iface the usrp-e interface object
 * \param clock the clock control interface
 * \param codec the codec control interface
 * \return a sptr to a new dboard interface
 */
uhd::usrp::dboard_iface::sptr make_usrp_e100_dboard_iface(
    e100_iface::sptr iface,
    e100_clock_ctrl::sptr clock,
    e100_codec_ctrl::sptr codec
);

/*!
 * Simple wax obj proxy class:
 * Provides a wax obj interface for a set and a get function.
 * This allows us to create nested properties structures
 * while maintaining flattened code within the implementation.
 */
class wax_obj_proxy : public wax::obj{
public:
    typedef boost::function<void(const wax::obj &, wax::obj &)>       get_t;
    typedef boost::function<void(const wax::obj &, const wax::obj &)> set_t;
    typedef boost::shared_ptr<wax_obj_proxy> sptr;

    static sptr make(const get_t &get, const set_t &set){
        return sptr(new wax_obj_proxy(get, set));
    }

private:
    get_t _get; set_t _set;
    wax_obj_proxy(const get_t &get, const set_t &set): _get(get), _set(set){};
    void get(const wax::obj &key, wax::obj &val){return _get(key, val);}
    void set(const wax::obj &key, const wax::obj &val){return _set(key, val);}
};

/*!
 * USRP-E100 implementation guts:
 * The implementation details are encapsulated here.
 * Handles properties on the mboard, dboard, dsps...
 */
class e100_impl : public uhd::device{
public:
    //structors
    e100_impl(
        const uhd::device_addr_t &,
        e100_iface::sptr,
        e100_clock_ctrl::sptr
    );
    ~e100_impl(void);

    //the io interface
    size_t send(const send_buffs_type &, size_t, const uhd::tx_metadata_t &, const uhd::io_type_t &, send_mode_t, double);
    size_t recv(const recv_buffs_type &, size_t, uhd::rx_metadata_t &, const uhd::io_type_t &, recv_mode_t, double);
    bool recv_async_msg(uhd::async_metadata_t &, double);
    size_t get_max_send_samps_per_packet(void) const;
    size_t get_max_recv_samps_per_packet(void) const;

private:
    //interface to ioctls and file descriptor
    e100_iface::sptr _iface;

    //ad9522 clock control
    e100_clock_ctrl::sptr _clock_ctrl;

    //ad9862 codec control
    e100_codec_ctrl::sptr _codec_ctrl;

    //handle io stuff
    uhd::transport::zero_copy_if::sptr _data_transport;
    UHD_PIMPL_DECL(io_impl) _io_impl;
    size_t _recv_frame_size, _send_frame_size;
    uhd::otw_type_t _send_otw_type, _recv_otw_type;
    void io_init(void);
    void handle_irq(void);
    void handle_overrun(size_t);
    void update_xport_channel_mapping(void);

    //configuration shadows
    uhd::clock_config_t _clock_config;

    //device functions and settings
    void get(const wax::obj &, wax::obj &);
    void set(const wax::obj &, const wax::obj &);

    //mboard functions and settings
    void mboard_init(void);
    void mboard_get(const wax::obj &, wax::obj &);
    void mboard_set(const wax::obj &, const wax::obj &);
    wax_obj_proxy::sptr _mboard_proxy;
    uhd::usrp::subdev_spec_t _rx_subdev_spec, _tx_subdev_spec;

    //xx dboard functions and settings
    void dboard_init(void);
    uhd::usrp::dboard_manager::sptr _dboard_manager;
    uhd::usrp::dboard_iface::sptr _dboard_iface;

    //rx dboard functions and settings
    uhd::usrp::dboard_eeprom_t _rx_db_eeprom;
    void rx_dboard_get(const wax::obj &, wax::obj &);
    void rx_dboard_set(const wax::obj &, const wax::obj &);
    wax_obj_proxy::sptr _rx_dboard_proxy;

    //tx dboard functions and settings
    uhd::usrp::dboard_eeprom_t _tx_db_eeprom, _gdb_eeprom;
    void tx_dboard_get(const wax::obj &, wax::obj &);
    void tx_dboard_set(const wax::obj &, const wax::obj &);
    wax_obj_proxy::sptr _tx_dboard_proxy;

    //methods and shadows for the dsps
    UHD_PIMPL_DECL(dsp_impl) _dsp_impl;
    void dsp_init(void);
    void issue_ddc_stream_cmd(const uhd::stream_cmd_t &, size_t);

    //properties interface for ddc
    void ddc_get(const wax::obj &, wax::obj &, size_t);
    void ddc_set(const wax::obj &, const wax::obj &, size_t);
    uhd::dict<std::string, wax_obj_proxy::sptr> _rx_dsp_proxies;

    //properties interface for duc
    void duc_get(const wax::obj &, wax::obj &, size_t);
    void duc_set(const wax::obj &, const wax::obj &, size_t);
    uhd::dict<std::string, wax_obj_proxy::sptr> _tx_dsp_proxies;

    //codec functions and settings
    void codec_init(void);
    void rx_codec_get(const wax::obj &, wax::obj &);
    void rx_codec_set(const wax::obj &, const wax::obj &);
    void tx_codec_get(const wax::obj &, wax::obj &);
    void tx_codec_set(const wax::obj &, const wax::obj &);
    wax_obj_proxy::sptr _rx_codec_proxy, _tx_codec_proxy;
    
    //clock control functions and settings
    void init_clock_config(void);
    void update_clock_config(void);
};

#endif /* INCLUDED_E100_IMPL_HPP */
