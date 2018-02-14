/*
 * HEIF codec.
 * Copyright (c) 2017 struktur AG, Dirk Farin <farin@struktur.de>
 *
 * This file is part of libheif.
 *
 * libheif is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * libheif is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with libheif.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <math.h>

#include "heif_hevc.h"
#include "bitstream.h"

using namespace heif;


static double read_depth_rep_info_element(BitReader& reader)
{
  int sign_flag = reader.get_bits(1);
  int exponent  = reader.get_bits(7);
  int mantissa_len = reader.get_bits(5)+1;
  if (mantissa_len<1 || mantissa_len>32) {
    // TODO err
  }

  if (exponent==127) {
    // TODO value unspecified
  }

  int mantissa = reader.get_bits(mantissa_len);
  double value;

  //printf("sign:%d exponent:%d mantissa_len:%d mantissa:%d\n",sign_flag,exponent,mantissa_len,mantissa);

  if (exponent > 0) {
    value = pow(2, exponent-31) * (1.0 + mantissa / pow(2,mantissa_len));
  }
  else {
    value = pow(2, -(30+mantissa_len)) * mantissa;
  }

  if (sign_flag) {
    value = -value;
  }

  return value;
}


static std::shared_ptr<SEIMessage> read_depth_representation_info(BitReader& reader)
{
  auto msg = std::make_shared<SEIMessage_depth_representation_info>();


  // default values

  msg->version = 1;

  msg->disparity_reference_view = 0;
  msg->depth_nonlinear_representation_model_size = 0;
  msg->depth_nonlinear_representation_model = nullptr;


  // read header

  msg->has_z_near = (uint8_t)reader.get_bits(1);
  msg->has_z_far  = (uint8_t)reader.get_bits(1);
  msg->has_d_min  = (uint8_t)reader.get_bits(1);
  msg->has_d_max  = (uint8_t)reader.get_bits(1);

  int rep_type;
  if (!reader.get_uvlc(&rep_type)) {
    // TODO error
  }
  // TODO: check rep_type range
  msg->depth_representation_type = (enum heif_depth_representation_type)rep_type;

  //printf("flags: %d %d %d %d\n",msg->has_z_near,msg->has_z_far,msg->has_d_min,msg->has_d_max);
  //printf("type: %d\n",rep_type);

  if (msg->has_d_min || msg->has_d_max) {
    int ref_view;
    if (!reader.get_uvlc(&ref_view)) {
      // TODO error
    }
    msg->disparity_reference_view = ref_view;

    //printf("ref_view: %d\n",msg->disparity_reference_view);
  }

  if (msg->has_z_near) msg->z_near = read_depth_rep_info_element(reader);
  if (msg->has_z_far ) msg->z_far  = read_depth_rep_info_element(reader);
  if (msg->has_d_min ) msg->d_min  = read_depth_rep_info_element(reader);
  if (msg->has_d_max ) msg->d_max  = read_depth_rep_info_element(reader);

  /*
  printf("z_near: %f\n",msg->z_near);
  printf("z_far: %f\n",msg->z_far);
  printf("dmin: %f\n",msg->d_min);
  printf("dmax: %f\n",msg->d_max);
  */

  if (msg->depth_representation_type == heif_depth_representation_type_nonuniform_disparity) {
    // TODO: load non-uniform response curve
  }

  return msg;
}


// aux subtypes: 00 00 00 11 / 00 00 00 0d / 4e 01 / b1 09 / 35 1e 78 c8 01 03 c5 d0 20

Error heif::decode_hevc_aux_sei_messages(const std::vector<uint8_t>& data,
                                         std::vector<std::shared_ptr<SEIMessage>>& msgs)
{
  // TODO: we probably do not need a full BitReader just for the array size.
  // Read this and the NAL size directly on the array data.

  BitReader reader(data.data(), (int)data.size());
  uint32_t len = (uint32_t)reader.get_bits(32);

  if (len > data.size()-4) {
    // ERROR: read past end of data
  }

  while (reader.get_current_byte_index() < (int)len) {
    int currPos = reader.get_current_byte_index();
    BitReader sei_reader(data.data() + currPos, (int)data.size()-currPos);

    uint32_t nal_size = (uint32_t)sei_reader.get_bits(32);
    (void)nal_size;

    uint8_t nal_type = (uint8_t)(sei_reader.get_bits(8) >> 1);
    sei_reader.skip_bits(8);

    // SEI

    if (nal_type == 39 ||
        nal_type == 40) {

      // TODO: loading of multi-byte sei headers
      uint8_t payload_id = (uint8_t)(sei_reader.get_bits(8));
      uint8_t payload_size = (uint8_t)(sei_reader.get_bits(8));
      (void)payload_size;

      switch (payload_id) {
      case 177: // depth_representation_info
        std::shared_ptr<SEIMessage> sei = read_depth_representation_info(sei_reader);
        msgs.push_back(sei);
        break;
      }
    }

    break; // TODO: read next SEI
  }


  return Error::Ok;
}
