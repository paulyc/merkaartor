#ifndef GGL_PROJECTIONS_CC_HPP
#define GGL_PROJECTIONS_CC_HPP

// Generic Geometry Library - projections (based on PROJ4)
// This file is automatically generated. DO NOT EDIT.

// Copyright Barend Gehrels (1995-2009), Geodan Holding B.V. Amsterdam, the Netherlands.
// Copyright Bruno Lalande (2008-2009)
// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// This file is converted from PROJ4, http://trac.osgeo.org/proj
// PROJ4 is originally written by Gerald Evenden (then of the USGS)
// PROJ4 is maintained by Frank Warmerdam
// PROJ4 is converted to Geometry Library by Barend Gehrels (Geodan, Amsterdam)

// Original copyright notice:

// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:

// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.

// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
// DEALINGS IN THE SOFTWARE.

#include <ggl/projections/impl/base_static.hpp>
#include <ggl/projections/impl/base_dynamic.hpp>
#include <ggl/projections/impl/projects.hpp>
#include <ggl/projections/impl/factory_entry.hpp>

namespace ggl { namespace projection
{
    #ifndef DOXYGEN_NO_IMPL
    namespace impl { namespace cc{
            static const double EPS10 = 1.e-10;

            struct par_cc
            {
                double ap;
            };

            // template class, using CRTP to implement forward/inverse
            template <typename LatLong, typename Cartesian, typename Parameters>
            struct base_cc_spheroid : public base_t_fi<base_cc_spheroid<LatLong, Cartesian, Parameters>, LatLong, Cartesian, Parameters>
            {

                typedef typename base_t_fi<base_cc_spheroid<LatLong, Cartesian, Parameters>, LatLong, Cartesian, Parameters>::LL_T LL_T;
                typedef typename base_t_fi<base_cc_spheroid<LatLong, Cartesian, Parameters>, LatLong, Cartesian, Parameters>::XY_T XY_T;

                par_cc m_proj_parm;

                inline base_cc_spheroid(const Parameters& par)
                    : base_t_fi<base_cc_spheroid<LatLong, Cartesian, Parameters>, LatLong, Cartesian, Parameters>(*this, par) {}

                inline void fwd(LL_T& lp_lon, LL_T& lp_lat, XY_T& xy_x, XY_T& xy_y) const
                {
                    if (fabs(fabs(lp_lat) - HALFPI) <= EPS10) throw proj_exception();;
                    xy_x = lp_lon;
                    xy_y = tan(lp_lat);
                }

                inline void inv(XY_T& xy_x, XY_T& xy_y, LL_T& lp_lon, LL_T& lp_lat) const
                {
                    lp_lat = atan(xy_y);
                    lp_lon = xy_x;
                }
            };

            // Central Cylindrical
            template <typename Parameters>
            void setup_cc(Parameters& par, par_cc& proj_parm)
            {
                par.es = 0.;
                // par.inv = s_inverse;
                // par.fwd = s_forward;
            }

        }} // namespace impl::cc
    #endif // doxygen

    /*!
        \brief Central Cylindrical projection
        \ingroup projections
        \tparam LatLong latlong point type
        \tparam Cartesian xy point type
        \tparam Parameters parameter type
        \par Projection characteristics
         - Cylindrical
         - Spheroid
        \par Example
        \image html ex_cc.gif
    */
    template <typename LatLong, typename Cartesian, typename Parameters = parameters>
    struct cc_spheroid : public impl::cc::base_cc_spheroid<LatLong, Cartesian, Parameters>
    {
        inline cc_spheroid(const Parameters& par) : impl::cc::base_cc_spheroid<LatLong, Cartesian, Parameters>(par)
        {
            impl::cc::setup_cc(this->m_par, this->m_proj_parm);
        }
    };

    #ifndef DOXYGEN_NO_IMPL
    namespace impl
    {

        // Factory entry(s)
        template <typename LatLong, typename Cartesian, typename Parameters>
        class cc_entry : public impl::factory_entry<LatLong, Cartesian, Parameters>
        {
            public :
                virtual projection<LatLong, Cartesian>* create_new(const Parameters& par) const
                {
                    return new base_v_fi<cc_spheroid<LatLong, Cartesian, Parameters>, LatLong, Cartesian, Parameters>(par);
                }
        };

        template <typename LatLong, typename Cartesian, typename Parameters>
        inline void cc_init(impl::base_factory<LatLong, Cartesian, Parameters>& factory)
        {
            factory.add_to_factory("cc", new cc_entry<LatLong, Cartesian, Parameters>);
        }

    } // namespace impl
    #endif // doxygen

}} // namespace ggl::projection

#endif // GGL_PROJECTIONS_CC_HPP
