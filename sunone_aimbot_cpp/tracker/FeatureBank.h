#ifndef FEATURE_BANK_H
#define FEATURE_BANK_H

#include <vector>
#include <deque>

/**
 * @brief Feature bank for storing and managing appearance features
 * 
 * Maintains a history of appearance features for each track,
 * computes representative features using EMA (Exponential Moving Average),
 * and filters features based on quality.
 */
class FeatureBank
{
public:
    struct Config
    {
        int maxSize = 100;              // Maximum number of features to store
        float emaAlpha = 0.9f;          // EMA smoothing coefficient (higher = more weight to new features)
        float qualityThreshold = 0.5f;  // Minimum quality to accept a feature
    };
    
    FeatureBank(const Config& config = Config());
    
    /**
     * @brief Add a new feature to the bank
     * @param feature Feature vector (should be normalized)
     * @param quality Feature quality score [0, 1]
     */
    void addFeature(const std::vector<float>& feature, float quality);
    
    /**
     * @brief Get the representative feature (EMA smoothed)
     * @return Smoothed feature vector
     */
    std::vector<float> getRepresentativeFeature() const;
    
    /**
     * @brief Get average feature quality
     * @return Average quality score [0, 1]
     */
    float getAverageQuality() const;
    
    /**
     * @brief Clear all features
     */
    void clear();
    
    /**
     * @brief Get number of features in bank
     */
    int size() const;
    
    /**
     * @brief Check if bank is empty
     */
    bool empty() const;
    
private:
    Config m_config;
    
    // FIFO queue of features
    std::deque<std::vector<float>> m_features;
    
    // Quality scores for each feature
    std::deque<float> m_qualities;
    
    // EMA smoothed feature (representative)
    std::vector<float> m_emaFeature;
    
    /**
     * @brief Update EMA feature with new feature
     * @param newFeature New feature to incorporate
     * @param quality Quality weight for the new feature
     */
    void updateEMA(const std::vector<float>& newFeature, float quality);
};

#endif // FEATURE_BANK_H
