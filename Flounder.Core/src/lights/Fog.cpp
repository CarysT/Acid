#include "fog.hpp"

namespace Flounder
{
	fog::fog(Colour *colour, const float &density, const float &gradient, const float &lowerLimit, const float &upperLimit) :
		m_colour(colour),
		m_density(density),
		m_gradient(gradient),
		m_lowerLimit(lowerLimit),
		m_upperLimit(upperLimit)
	{
	}

	fog::~fog()
	{
		delete m_colour;
	}
}