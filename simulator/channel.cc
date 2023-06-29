/**
 * @file channel.h
 * @brief Implementation file for the channel class
 */
#include "channel.h"
#include "logger.h"

#include <utility>

static constexpr bool kPrintChannelOutput = false;
static constexpr bool kPrintSNRCheck = false;
static constexpr double kMeanChannelGain = 0.1f;

Channel::~Channel() = default;

Channel::Channel(const Config* const config, std::string& in_channel_type,
                 double in_channel_snr,  std::string& dataset_path )
    : cfg_(config),
      sim_chan_model_(std::move(in_channel_type)),
      channel_snr_db_(in_channel_snr),
      dataset_path_(dataset_path){

  channel_model = GetChannelModel();

  float snr_lin = std::pow(10, channel_snr_db_ / 10.0f);
  noise_samp_std_ = std::sqrt(kMeanChannelGain / (snr_lin * 2.0f));
  std::cout << "Noise level to be used is: " << std::fixed << std::setw(5)
            << std::setprecision(2) << noise_samp_std_ << std::endl;
}

ChannelModel* Channel::GetChannelModel()
{

  if( sim_chan_model_ == "AWGN" ) return new AwgnModel( cfg_ );
  
  if( sim_chan_model_ == "RAYLEIGH" ) return new RayleighModel( cfg_ );

  if( sim_chan_model_ == "DATASET" ) return new DatasetModel( cfg_ , dataset_path_ );
  
  AGORA_LOG_WARN("Invalid channel model at CHSim, assuming AWGN... \n");

  return new AwgnModel( cfg_ );

}

void Channel::ApplyChan(const arma::cx_fmat& fmat_src, arma::cx_fmat& fmat_dst,
                        const bool is_downlink, const bool is_newChan) {
  arma::cx_fmat fmat_h;

  if( is_newChan )
  {

    h_ = channel_model->GetAndUpdateMatrix();
    h_ = sqrt(kMeanChannelGain / 2.0f) * h_;

  }

  if (is_downlink) {
    fmat_h = fmat_src * h_.st(); 
  } else {
    fmat_h = fmat_src * h_; 
  }

  // Add noise
  Awgn(fmat_h, fmat_dst);
  
  if (kPrintChannelOutput) {
    Utils::PrintMat(h_, "H");
  }

}

void Channel::Awgn(const arma::cx_fmat& src, arma::cx_fmat& dst) const {
  if (channel_snr_db_ < 120.0f) {
    const int n_row = src.n_rows;
    const int n_col = src.n_cols;

    // Generate noise
    arma::cx_fmat noise(arma::randn<arma::fmat>(n_row, n_col),
                        arma::randn<arma::fmat>(n_row, n_col));

    // Supposed to be faster
    // arma::fmat x(n_row, n_col, arma::fill::arma::randn);
    // arma::fmat y(n_row, n_col, arma::fill::arma::randn);
    // arma::cx_fmat noise = arma::cx_fmat(x, y);

    // Add noise to signal
    noise *= noise_samp_std_;
    dst = src + noise;

    // Check SNR
    if (kPrintSNRCheck) {
      arma::fmat noise_sq = arma::square(abs(noise));
      arma::frowvec noise_vec = arma::mean(noise_sq, 0);
      arma::fmat src_sq = arma::square(abs(src));
      arma::frowvec pwr_vec = arma::mean(src_sq, 0);
      arma::frowvec snr = 10.0f * arma::log10(pwr_vec / noise_vec);
      std::cout << "SNR: " << snr;
    }
  } else {
    dst = src;
  }
}