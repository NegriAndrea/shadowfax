/*******************************************************************************
 * This file is part of Shadowfax
 * Copyright (C) 2016 Bert Vandenbroucke (bert.vandenbroucke@gmail.com)
 *                    Sven De Rijcke (sven.derijcke@ugent.be)
 *
 * Shadowfax is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Shadowfax is distributed in the hope that it will be useful,
 * but WITOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Shadowfax. If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

/**
 * @file DiscreteStellarFeedback.cpp
 *
 * @brief StellarFeedback implementation that takes into account the discrete
 * nature of stellar feedback: implementation
 *
 * @author Bert Vandenbroucke (bert.vandenbroucke@ugent.be)
 * @author Sven De Rijcke (sven.derijcke@ugent.be)
 */
#include "DiscreteStellarFeedback.hpp"
#include "RestartFile.hpp"
#include <cmath>
using namespace std;

/**
 * @brief Low mass part of the Chabrier IMF
 *
 * @param m Mass value
 * @return Value of the low-mass part of the Chabrier IMF
 */
double DiscreteStellarFeedback::PopII_IMF_sub1(double m) {
    double a = log10(m) + 1.1024;
    return exp(-0.5 * (a * a / 0.4761)) / m;
}

/**
 * @brief Chabrier IMF
 *
 * @param m Mass value
 * @return Value of the Chabrier IMF
 */
double DiscreteStellarFeedback::PopII_IMF(double m) {
    if(m > _PopII_M_low && m < _PopII_M_upp) {
        if(m < 1.) {
            return _PopII_fac_IMF * PopII_IMF_sub1(m);
        } else {
            return pow(m, -2.3);
        }
    } else {
        return 0.;
    }
}

/**
 * @brief Chabrier IMF mass integrand
 *
 * @param m Mass value
 * @return Integrand for the Chabrier IMF mass integral
 */
double DiscreteStellarFeedback::PopII_mIMF(double m) {
    return m * PopII_IMF(m);
}

/**
 * @brief First part of the SNIa delay time distribution
 *
 * See Mannucci et al. 2006 (MNRAS, 370, pp. 773-783)
 *
 * @param t Time value
 * @return First part of the SNIa delay time distribution
 */
double DiscreteStellarFeedback::PopII_SNIa_delay1(double t) {
    double a = (t - _PopII_SNIa_delay_mu) / _PopII_SNIa_delay_sigma;
    return _PopII_SNIa_delay_norm1 * (t - 0.03) * (13.6 - t) *
           exp(-0.5 * a * a);
}

/**
 * @brief Second part of the SNIa delay time distribution
 *
 * See Mannucci et al. 2006 (MNRAS, 370, pp. 773-783)
 *
 * @param t Time value
 * @return Second part of the SNIa delay time distribution
 */
double DiscreteStellarFeedback::PopII_SNIa_delay2(double t) {
    double delay;
    if(t < 0.25) {
        delay = _PopII_SNIa_delay_norm2 *
                (exp((t - 0.25) / 0.1) - exp((0.03 - 0.25) / 0.1));
    } else {
        delay = _PopII_SNIa_delay_norm2 *
                (exp((0.25 - t) / 7.) - exp((0.03 - 0.25) / 0.1));
    }

    if(delay > 0.) {
        return delay;
    } else {
        return 0.;
    }
}

/**
 * @brief SNIa delay time distribution
 *
 * See Mannucci et al. 2006 (MNRAS, 370, pp. 773-783)
 *
 * @param t Time value
 * @return SNIa delay time distribution
 */
double DiscreteStellarFeedback::PopII_SNIa_delay(double t) {
    return PopII_SNIa_delay1(t) + PopII_SNIa_delay2(t);
}

/**
 * @brief Susa PopIII IMF
 *
 * @param m Mass value
 * @return Value of the Susa Pop III IMF
 */
double DiscreteStellarFeedback::PopIII_IMF(double m) {
    if(m > _PopIII_M_low && m < _PopIII_M_upp) {
        double logm = log10(m);
        double IMF;
        if(logm < _PopIII_M2) {
            IMF = 0.5 * (logm - _PopIII_M1) / (_PopIII_M2 - _PopIII_M1);
        } else {
            IMF = 0.5 * (logm + _PopIII_M3 - 2. * _PopIII_M2) /
                  (_PopIII_M3 - _PopIII_M2);
        }
        IMF = _PopIII_fac * pow(IMF * (1.0 - IMF), _PopIII_pw);
        if(IMF > 0.) {
            return IMF / m;
        } else {
            return 0.;
        }
    } else {
        return 0.;
    }
}

/**
 * @brief Susa PopIII IMF mass integrand
 *
 * @param m Mass value
 * @return Integrand for the Susa PopIII IMF mass integral
 */
double DiscreteStellarFeedback::PopIII_mIMF(double m) {
    return m * PopIII_IMF(m);
}

/**
 * @brief Energy of a PopIII SN with a given mass
 *
 * @param m Mass value
 * @return Energy of a PopIII SN with that mass
 */
double DiscreteStellarFeedback::PopIII_E_SN(double m) {
    return 1.e51 * _PopIII_E_SN_spline->eval(m);
}

/**
 * @brief Integrand of the PopIII IMF energy integral
 *
 * @param m Mass value
 * @return Integrand value
 */
double DiscreteStellarFeedback::PopIII_EIMF(double m) {
    return PopIII_E_SN(m) * PopIII_IMF(m);
}

/**
 * @brief Initialize the internal variables
 */
void DiscreteStellarFeedback::initialize() {
    // Chabrier IMF: mass interval and normalization factor for low mass part
    _PopII_M_low = 0.07;
    _PopII_M_upp = 100.;
    _PopII_M_SNII_low = 8.;
    _PopII_M_SNIa_low = 3.;
    _PopII_M_SNIa_upp = 8.;
    _PopII_fac_IMF = 1. / PopII_IMF_sub1(1.);

    // SNIa delay time distribution
    _PopII_SNIa_delay_mu = 0.05;
    _PopII_SNIa_delay_sigma = 0.01;
    _PopII_SNIa_delay_norm1 = 1.;
    _PopII_SNIa_delay_norm2 = 1.;

    // Cutoff metallicity for PopIII stars
    _PopIII_cutoff = -5.;

    // PopIII IMF: mass interval and factors (come from Sven's Python script)
    _PopIII_M_low = 0.7;
    _PopIII_M_upp = 500.;
    _PopIII_M_SN_low = 10.;
    _PopIII_M1 = log10(_PopIII_M_low);
    _PopIII_M2 = 1.51130759;
    _PopIII_M3 = log10(_PopIII_M_upp);
    _PopIII_fac = 708.92544818;
    _PopIII_pw = 2.8008394;

    // Calculate number of SNII, SNIa and PopIII SN by integrating the IMFs
    _PopII_Mint = GSL::qag(&PopII_mIMF_static, _PopII_M_low, _PopII_M_upp,
                           1.e-8, this);
    _PopII_NIIint = GSL::qag(&PopII_IMF_static, _PopII_M_SNII_low, _PopII_M_upp,
                             1.e-8, this);
    _PopII_NIaint = GSL::qag(&PopII_IMF_static, _PopII_M_SNIa_low,
                             _PopII_M_SNIa_upp, 1.e-8, this);

    _PopIII_Mint = GSL::qag(&PopIII_mIMF_static, _PopIII_M_low, _PopIII_M_upp,
                            1.e-8, this);
    _PopIII_Nint = GSL::qag(&PopIII_IMF_static, _PopIII_M_SN_low, _PopIII_M_upp,
                            1.e-8, this);

    // Normalize the SNIa delay time function
    double dint = GSL::qag(&PopII_SNIa_delay1_static, 0.03, 13.6, 1.e-8, this);
    _PopII_SNIa_delay_norm1 = 0.4 / dint;
    dint = GSL::qag(&PopII_SNIa_delay2_static, 0.03, 13.6, 1.e-8, this);
    _PopII_SNIa_delay_norm2 = 0.6 / dint;

    // Initialize the cumulative SNIa delay time spline
    double ts[30], cumul_delay[30];
    unsigned int i = 1;
    double t;
    ts[0] = -2.;
    cumul_delay[0] = 0.;
    for(t = log10(0.03); t < log10(13.6); t += 0.1) {
        ts[i] = t;
        cumul_delay[i] = GSL::qag(&PopII_SNIa_delay_static, 0.03, pow(10., t),
                                  1.e-8, this);
        i++;
    }
    ts[28] = log10(13.6);
    cumul_delay[28] = 1.;
    ts[29] = log10(13.8);
    cumul_delay[29] = 1.;
    ts[0] += cumul_delay[1];
    _PopII_SNIa_delay_spline = GSL::GSLInterpolator::create(
            GSL::TypeGSLCubicSplineInterpolator, ts, cumul_delay, 30);

    // Initialize the cumulative PopIII IMF spline
    // We need this many samples to come reliably close to Sven's intervals
    // without needing to evaluate the cumulative distribution at runtime
    double PopIII_IMF_ms[289], PopIII_IMF_IMFs[289];
    i = 1;
    double m;
    PopIII_IMF_ms[0] = -2.;
    PopIII_IMF_IMFs[0] =
            GSL::qag(&PopIII_IMF_static, 0., _PopIII_M_upp, 1.e-8, this);
    for(m = log10(_PopIII_M_low); m < log10(_PopIII_M_upp); m += 0.01) {
        PopIII_IMF_ms[i] = m;
        PopIII_IMF_IMFs[i] = GSL::qag(&PopIII_IMF_static, pow(10., m),
                                      _PopIII_M_upp, 1.e-8, this);
        i++;
    }
    PopIII_IMF_ms[287] = log10(_PopIII_M_upp);
    PopIII_IMF_IMFs[287] = 0.;
    PopIII_IMF_ms[288] = 3.;
    PopIII_IMF_IMFs[288] = 0.;
    _PopIII_IMF_spline =
            GSL::GSLInterpolator::create(GSL::TypeGSLLinearInterpolator,
                                         PopIII_IMF_ms, PopIII_IMF_IMFs, 289);

    // Initialize PopII lifetime spline using Sven's data
    double PopII_lifetime_ms[87] = {
            0.65, 0.7,  0.75, 0.8,  0.85, 0.9,  0.95, 1,    1.05, 1.1,  1.15,
            1.2,  1.25, 1.3,  1.35, 1.4,  1.45, 1.5,  1.55, 1.6,  1.65, 1.7,
            1.75, 1.8,  1.85, 1.9,  1.95, 2,    2.05, 2.1,  2.15, 2.2,  2.25,
            2.3,  2.35, 2.4,  2.6,  2.8,  3,    3.2,  3.4,  3.6,  3.8,  4,
            4.2,  4.4,  4.6,  4.8,  5,    5.2,  5.4,  5.6,  5.8,  6,    6.2,
            6.4,  7,    8,    9,    10,   11,   12,   14,   16,   18,   20,
            24,   28,   30,   40,   45,   50,   55,   60,   65,   70,   75,
            80,   90,   95,   100,  120,  150,  200,  250,  300,  350};
    double PopII_lifetime_ts[87] = {
            10.452,  10.3415, 10.2362, 10.139,  10.0468, 9.95885, 9.87605,
            9.79803, 9.72399, 9.65217, 9.58335, 9.52138, 9.46703, 9.42591,
            9.38266, 9.33495, 9.28953, 9.24572, 9.20422, 9.16552, 9.12806,
            9.0921,  9.05728, 9.02345, 8.99101, 8.95935, 8.92926, 8.90003,
            8.87177, 8.84446, 8.81782, 8.79194, 8.7669,  8.74255, 8.7187,
            8.69608, 8.60937, 8.53033, 8.45752, 8.39064, 8.32889, 8.27122,
            8.21763, 8.16709, 8.12,    8.07545, 8.03334, 7.99336, 7.95561,
            7.91966, 7.88551, 7.85298, 7.82191, 7.79234, 7.76385, 7.73664,
            7.66115, 7.55343, 7.46282, 7.38582, 7.31927, 7.26113, 7.16375,
            7.08681, 7.02325, 6.97053, 6.88722, 6.82377, 6.79741, 6.72284,
            6.68824, 6.65918, 6.63473, 6.61362, 6.59507, 6.57899, 6.56445,
            6.55241, 6.52965, 6.52028, 6.5111,  6.48241, 6.45054, 6.41528,
            6.3914,  6.37373, 6.36056};
    _PopII_lifetime_spline = GSL::GSLInterpolator::create(
            GSL::TypeGSLCubicSplineInterpolator, PopII_lifetime_ms,
            PopII_lifetime_ts, 87);

    // Initialize PopIII lifetime spline using Sven's data
    double PopIII_lifetime_ms[24] = {0.7, 0.8, 0.9, 1,  1.1, 1.2, 1.3, 1.4,
                                     1.5, 1.6, 1.8, 2,  2.5, 3,   4,   5,
                                     10,  15,  20,  30, 50,  70,  100, 500};
    double PopIII_lifetime_ts[24] = {
            1.32077,   1.13354,   0.947924,  0.78533,  0.637463,  0.516384,
            0.399708,  0.289454,  0.193159,  0.100413, -0.062413, -0.208425,
            -0.493919, -0.702468, -0.979499, -1.18585, -1.75003,  -1.96392,
            -2.08837,  -2.24245,  -2.39566,  -2.47468, -2.54276,  -2.89056};
    _PopIII_lifetime_spline = GSL::GSLInterpolator::create(
            GSL::TypeGSLCubicSplineInterpolator, PopIII_lifetime_ms,
            PopIII_lifetime_ts, 24);

    // Initialize PopIII SN energy spline using Woosley energy data
    // Calculate PopIII SN energy integral
    double ms[18] = {0.7,  10.,  35.,  40.,  50.,  60.,
                     70.,  80.,  90.,  100., 140., 140. + 1.e-10,
                     150., 170., 200., 270., 300., 500.};
    double Es[18] = {0., 1., 1., 1.,  1.,  1.,  1.,  1.,  1.,
                     1., 1., 9., 16., 28., 44., 49., 50., 50.};
    _PopIII_E_SN_spline = GSL::GSLInterpolator::create(
            GSL::TypeGSLLinearInterpolator, ms, Es, 18);
    _PopIII_Eint = GSL::qag(&PopIII_EIMF_static, _PopIII_M_SN_low,
                            _PopIII_M_upp, 1.e-8, this);

    // Initialize feedback values
    double feedback_efficiency = 0.7;
    // NEED TO CONVERT TO INTERNAL ENERGY UNIT!!!
    _PopII_SNII_energy = 1.e51 * feedback_efficiency;
    _PopII_SNII_mass = 0.191445322565;
    _PopII_SNII_metals = 0.0241439721018;
    _PopII_SNII_Fe = 0.000932719658516;
    _PopII_SNII_Mg = 0.00151412640705;

    // NEED TO CONVERT TO INTERNAL ENERGY UNIT!!!
    _PopII_SNIa_energy = 1.e51 * feedback_efficiency;
    _PopII_SNIa_mass = 0.00655147325196;
    _PopII_SNIa_metals = 0.00655147325196;
    _PopII_SNIa_Fe = 0.00165100587997;
    _PopII_SNIa_Mg = 0.000257789470044;

    // NEED TO CONVERT TO INTERNAL ENERGY UNIT!!!
    _PopII_SW_energy = 1.e50 * feedback_efficiency;
    // NEED TO CONVERT TO INTERNAL TIME UNIT!!!
    _PopII_SW_end_time = 31.0;
    _PopII_SW_energy /= _PopII_SW_end_time;

    // NEED TO CONVERT TO INTERNAL ENERGY UNIT!!!
    _PopIII_SN_energy = _PopIII_Eint * feedback_efficiency;
    _PopIII_SN_mass = 0.45;
    _PopIII_SN_metals = 0.026;
    _PopIII_SN_Fe = 0.0000932719658516;
    _PopIII_SN_Mg = 0.000151412640705;

    // NEED TO CONVERT TO INTERNAL ENERGY UNIT!!!
    _PopIII_SW_energy = 1.e51 * feedback_efficiency;
    // NEED TO CONVERT TO INTERNAL TIME UNIT!!!
    _PopIII_SW_end_time = 16.7;
    _PopIII_SW_energy /= _PopIII_SW_end_time;
}

/**
 * @brief Constructor
 *
 * Calls initialize().
 */
DiscreteStellarFeedback::DiscreteStellarFeedback() {
    initialize();
}

/**
 * @brief Give discrete stellar feedback
 *
 * @param star StarParticle that does feedback
 * @param particles ParticleVector containing the gas that receives feedback
 * @param dt Time interval over which the feedback is done
 */
void DiscreteStellarFeedback::do_feedback(StarParticle* star,
                                          ParticleVector& particles,
                                          double dt) {
    // needs to be implemented
}

/**
 * @brief Dump the object to the given RestartFile
 *
 * @param rfile RestartFile to write to
 */
void DiscreteStellarFeedback::dump(RestartFile& rfile) {
    rfile.write(_PopII_M_low);
    rfile.write(_PopII_M_upp);
    rfile.write(_PopII_fac_IMF);

    rfile.write(_PopII_M_SNII_low);

    rfile.write(_PopII_M_SNIa_low);
    rfile.write(_PopII_M_SNIa_upp);
    rfile.write(_PopII_SNIa_delay_mu);
    rfile.write(_PopII_SNIa_delay_sigma);
    rfile.write(_PopII_SNIa_delay_norm1);
    rfile.write(_PopII_SNIa_delay_norm2);

    rfile.write(_PopIII_cutoff);
    rfile.write(_PopIII_M_low);
    rfile.write(_PopIII_M_upp);
    rfile.write(_PopIII_M_SN_low);
    rfile.write(_PopIII_M1);
    rfile.write(_PopIII_M2);
    rfile.write(_PopIII_M3);
    rfile.write(_PopIII_fac);
    rfile.write(_PopIII_pw);

    rfile.write(_PopII_Mint);
    rfile.write(_PopII_NIIint);
    rfile.write(_PopII_NIaint);

    rfile.write(_PopIII_Mint);
    rfile.write(_PopIII_Nint);
    rfile.write(_PopIII_Eint);

    _PopII_SNIa_delay_spline->dump(rfile);

    _PopIII_IMF_spline->dump(rfile);

    _PopII_lifetime_spline->dump(rfile);
    _PopIII_lifetime_spline->dump(rfile);

    _PopIII_E_SN_spline->dump(rfile);

    rfile.write(_PopII_SNII_energy);
    rfile.write(_PopII_SNII_mass);
    rfile.write(_PopII_SNII_metals);
    rfile.write(_PopII_SNII_Fe);
    rfile.write(_PopII_SNII_Mg);

    rfile.write(_PopII_SNIa_energy);
    rfile.write(_PopII_SNIa_mass);
    rfile.write(_PopII_SNIa_metals);
    rfile.write(_PopII_SNIa_Fe);
    rfile.write(_PopII_SNIa_Mg);

    rfile.write(_PopII_SW_energy);
    rfile.write(_PopII_SW_end_time);

    rfile.write(_PopIII_SN_energy);
    rfile.write(_PopIII_SN_mass);
    rfile.write(_PopIII_SN_metals);
    rfile.write(_PopIII_SN_Fe);
    rfile.write(_PopIII_SN_Mg);

    rfile.write(_PopIII_SW_energy);
    rfile.write(_PopIII_SW_end_time);
}

/**
 * @brief Restart constructor
 *
 * @param rfile RestartFile to read from
 */
DiscreteStellarFeedback::DiscreteStellarFeedback(RestartFile& rfile) {
    rfile.read(_PopII_M_low);
    rfile.read(_PopII_M_upp);
    rfile.read(_PopII_fac_IMF);

    rfile.read(_PopII_M_SNII_low);

    rfile.read(_PopII_M_SNIa_low);
    rfile.read(_PopII_M_SNIa_upp);
    rfile.read(_PopII_SNIa_delay_mu);
    rfile.read(_PopII_SNIa_delay_sigma);
    rfile.read(_PopII_SNIa_delay_norm1);
    rfile.read(_PopII_SNIa_delay_norm2);

    rfile.read(_PopIII_cutoff);
    rfile.read(_PopIII_M_low);
    rfile.read(_PopIII_M_upp);
    rfile.read(_PopIII_M_SN_low);
    rfile.read(_PopIII_M1);
    rfile.read(_PopIII_M2);
    rfile.read(_PopIII_M3);
    rfile.read(_PopIII_fac);
    rfile.read(_PopIII_pw);

    rfile.read(_PopII_Mint);
    rfile.read(_PopII_NIIint);
    rfile.read(_PopII_NIaint);

    rfile.read(_PopIII_Mint);
    rfile.read(_PopIII_Nint);
    rfile.read(_PopIII_Eint);

    _PopII_SNIa_delay_spline = GSL::GSLInterpolator::create(rfile);

    _PopIII_IMF_spline = GSL::GSLInterpolator::create(rfile);

    _PopII_lifetime_spline = GSL::GSLInterpolator::create(rfile);
    _PopIII_lifetime_spline = GSL::GSLInterpolator::create(rfile);

    _PopIII_E_SN_spline = GSL::GSLInterpolator::create(rfile);

    rfile.read(_PopII_SNII_energy);
    rfile.read(_PopII_SNII_mass);
    rfile.read(_PopII_SNII_metals);
    rfile.read(_PopII_SNII_Fe);
    rfile.read(_PopII_SNII_Mg);

    rfile.read(_PopII_SNIa_energy);
    rfile.read(_PopII_SNIa_mass);
    rfile.read(_PopII_SNIa_metals);
    rfile.read(_PopII_SNIa_Fe);
    rfile.read(_PopII_SNIa_Mg);

    rfile.read(_PopII_SW_energy);
    rfile.read(_PopII_SW_end_time);

    rfile.read(_PopIII_SN_energy);
    rfile.read(_PopIII_SN_mass);
    rfile.read(_PopIII_SN_metals);
    rfile.read(_PopIII_SN_Fe);
    rfile.read(_PopIII_SN_Mg);

    rfile.read(_PopIII_SW_energy);
    rfile.read(_PopIII_SW_end_time);
}
