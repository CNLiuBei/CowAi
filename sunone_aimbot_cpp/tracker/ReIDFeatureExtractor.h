#ifndef REID_FEATURE_EXTRACTOR_H
#define REID_FEATURE_EXTRACTOR_H

#include <vector>
#include <string>
#include <memory>
#include <opencv2/opencv.hpp>

// Forward declarations for ONNX Runtime
namespace Ort
{
    struct Env;
    struct Session;
    struct SessionOptions;
    struct MemoryInfo;
}

/**
 * @brief ReID (Re-Identification) feature extractor using ONNX Runtime
 * 
 * Extracts appearance features from detected targets for matching.
 * Supports batch processing and GPU acceleration via DirectML.
 */
class ReIDFeatureExtractor
{
public:
    struct Config
    {
        std::string modelPath = "models/osnet_x0_25.onnx";
        int featureDim = 128;           // Output feature dimension
        int inputWidth = 128;           // Model input width
        int inputHeight = 256;          // Model input height
        bool useGPU = true;             // Use DirectML GPU acceleration
        int maxBatchSize = 50;          // Maximum batch size
        float minDetectionSize = 20.0f; // Minimum detection box size (pixels)
    };
    
    ReIDFeatureExtractor(const Config& config);
    ~ReIDFeatureExtractor();
    
    /**
     * @brief Extract feature from a single detection
     * @param frame Original frame
     * @param bbox Detection bounding box
     * @return Normalized feature vector (empty if extraction fails)
     */
    std::vector<float> extractFeature(const cv::Mat& frame, const cv::Rect& bbox);
    
    /**
     * @brief Extract features from multiple detections (batch processing)
     * @param frame Original frame
     * @param bboxes Detection bounding boxes
     * @return Feature vectors (empty vector for failed extractions)
     */
    std::vector<std::vector<float>> extractFeatures(
        const cv::Mat& frame,
        const std::vector<cv::Rect>& bboxes
    );
    
    /**
     * @brief Calculate cosine similarity between two features
     * @param feat1 First feature vector
     * @param feat2 Second feature vector
     * @return Similarity score [0, 1] (1 = identical, 0 = orthogonal)
     */
    static float cosineSimilarity(
        const std::vector<float>& feat1,
        const std::vector<float>& feat2
    );
    
    /**
     * @brief Normalize feature vector to unit length (L2 normalization)
     * @param feature Feature vector to normalize (modified in-place)
     */
    static void normalizeFeature(std::vector<float>& feature);
    
    /**
     * @brief Check if extractor is initialized
     */
    bool isInitialized() const { return m_initialized; }
    
private:
    Config m_config;
    bool m_initialized;
    
    // ONNX Runtime components
    std::unique_ptr<Ort::Env> m_ortEnv;
    std::unique_ptr<Ort::Session> m_ortSession;
    std::unique_ptr<Ort::SessionOptions> m_sessionOptions;
    std::unique_ptr<Ort::MemoryInfo> m_memoryInfo;
    
    // Model input/output names
    std::vector<const char*> m_inputNames;
    std::vector<const char*> m_outputNames;
    
    /**
     * @brief Initialize ONNX Runtime session
     * @return true if successful
     */
    bool initializeSession();
    
    /**
     * @brief Preprocess image crop for model input
     * @param crop Cropped image region
     * @return Preprocessed image (CHW format, normalized)
     */
    cv::Mat preprocessImage(const cv::Mat& crop);
    
    /**
     * @brief Run inference on preprocessed input
     * @param input Preprocessed image data (CHW format)
     * @return Feature vector
     */
    std::vector<float> runInference(const std::vector<float>& input);
    
    /**
     * @brief Run batch inference
     * @param inputs Batch of preprocessed images
     * @return Batch of feature vectors
     */
    std::vector<std::vector<float>> runBatchInference(
        const std::vector<std::vector<float>>& inputs
    );
    
    /**
     * @brief Validate and clip bounding box to frame bounds
     * @param bbox Input bounding box
     * @param frameSize Frame size
     * @return Clipped bounding box
     */
    static cv::Rect clipBBox(const cv::Rect& bbox, const cv::Size& frameSize);
};

#endif // REID_FEATURE_EXTRACTOR_H
