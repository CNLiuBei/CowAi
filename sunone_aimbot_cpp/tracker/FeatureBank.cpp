#include "FeatureBank.h"
#include <algorithm>
#include <numeric>
#include <cmath>

FeatureBank::FeatureBank(const Config& config)
    : m_config(config)
{
}

void FeatureBank::addFeature(const std::vector<float>& feature, float quality)
{
    // Filter out low-quality features
    if (quality < m_config.qualityThreshold)
        return;
    
    // Add to FIFO queue
    m_features.push_back(feature);
    m_qualities.push_back(quality);
    
    // Maintain max size (remove oldest if exceeded)
    if (m_features.size() > static_cast<size_t>(m_config.maxSize))
    {
        m_features.pop_front();
        m_qualities.pop_front();
    }
    
    // Update EMA feature
    updateEMA(feature, quality);
}

void FeatureBank::updateEMA(const std::vector<float>& newFeature, float quality)
{
    if (m_emaFeature.empty())
    {
        // Initialize EMA with first feature
        m_emaFeature = newFeature;
        return;
    }
    
    // Ensure feature dimensions match
    if (newFeature.size() != m_emaFeature.size())
        return;
    
    // Quality-weighted EMA: higher quality features get more weight
    float alpha = m_config.emaAlpha;
    
    // Adjust alpha based on quality (high quality = more weight)
    if (quality > 0.8f)
        alpha = std::min(0.95f, alpha * 1.1f);  // Increase weight for high quality
    else if (quality < 0.6f)
        alpha = std::max(0.7f, alpha * 0.9f);   // Decrease weight for lower quality
    
    // EMA update: ema = alpha * new + (1 - alpha) * ema
    for (size_t i = 0; i < m_emaFeature.size(); ++i)
    {
        m_emaFeature[i] = alpha * newFeature[i] + (1.0f - alpha) * m_emaFeature[i];
    }
    
    // Normalize EMA feature to maintain unit length
    float norm = 0.0f;
    for (float val : m_emaFeature)
        norm += val * val;
    norm = std::sqrt(norm);
    
    if (norm > 1e-6f)
    {
        for (float& val : m_emaFeature)
            val /= norm;
    }
}

std::vector<float> FeatureBank::getRepresentativeFeature() const
{
    if (m_emaFeature.empty() && !m_features.empty())
    {
        // Fallback: return most recent feature if EMA not initialized
        return m_features.back();
    }
    
    return m_emaFeature;
}

float FeatureBank::getAverageQuality() const
{
    if (m_qualities.empty())
        return 0.0f;
    
    float sum = std::accumulate(m_qualities.begin(), m_qualities.end(), 0.0f);
    return sum / static_cast<float>(m_qualities.size());
}

void FeatureBank::clear()
{
    m_features.clear();
    m_qualities.clear();
    m_emaFeature.clear();
}

int FeatureBank::size() const
{
    return static_cast<int>(m_features.size());
}

bool FeatureBank::empty() const
{
    return m_features.empty();
}
