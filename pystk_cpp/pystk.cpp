

//
//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2004-2015 Steve Baker <sjbaker1@airmail.net>
//  Copyright (C) 2011-2015 Joerg Henrichs, Marianne Gagnon
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.


#ifdef WIN32
#  ifdef __CYGWIN__
#    include <unistd.h>
#  endif
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  ifdef _MSC_VER
#    include <direct.h>
#  endif
#else
#  include <signal.h>
#  include <unistd.h>
#endif
#include <stdexcept>
#include <cstdio>
#include <string>
#include <cstring>
#include <sstream>
#include <algorithm>
#include <limits>

#include <IEventReceiver.h>

#include "pystk.hpp"
#include "config/stk_config.hpp"
#include "config/user_config.hpp"
#include "font/font_manager.hpp"
#include "graphics/camera.hpp"
#include "graphics/central_settings.hpp"
#include "graphics/frame_buffer.hpp"
#include "graphics/graphics_restrictions.hpp"
#include "graphics/irr_driver.hpp"
#include "graphics/material_manager.hpp"
#include "graphics/particle_kind_manager.hpp"
#include "graphics/referee.hpp"
#include "graphics/render_target.hpp"
#include "graphics/rtts.hpp"
#include "graphics/sp/sp_base.hpp"
#include "graphics/sp/sp_shader.hpp"
#include "graphics/sp/sp_texture_manager.hpp"
#include "input/input.hpp"
#include "io/file_manager.hpp"
#include "items/attachment_manager.hpp"
#include "items/item_manager.hpp"
#include "items/powerup_manager.hpp"
#include "items/projectile_manager.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/combined_characteristic.hpp"
#include "karts/controller/ai_base_lap_controller.hpp"
#include "karts/kart_model.hpp"
#include "karts/kart_properties.hpp"
#include "karts/kart_properties_manager.hpp"
#include "modes/world.hpp"
#include "physics/physics.hpp"
#include "race/race_manager.hpp"
#include "scriptengine/property_animator.hpp"
#include "tracks/arena_graph.hpp"
#include "tracks/track.hpp"
#include "tracks/track_manager.hpp"
#include "utils/command_line.hpp"
#include "utils/constants.hpp"
#include "utils/crash_reporting.hpp"
#include "utils/leak_check.hpp"
#include "utils/log.hpp"
#include "utils/mini_glm.hpp"
#include "utils/profiler.hpp"
#include "utils/string_utils.hpp"
#include "utils/objecttype.h"
#include "util.hpp"
#include "buffer.hpp"

#include "BulletCollision/CollisionDispatch/btCollisionWorld.h"

#ifdef RENDERDOC
#include "renderdoc_app.h"
#ifdef _WIN32
#include <windows.h>
#elif defined(__linux__)
#include <dlfcn.h>
#endif
#endif

const PySTKGraphicsConfig & PySTKGraphicsConfig::hd() {
    static PySTKGraphicsConfig config = {600,400,
        true, true, true, true, true, 
        2,     // particle_effects
        true,  // animated_characters
        true,  // motionblur
        true,  // mlaa
        true,  // texture_compression
        true,  // ssao
        false, // degraded_IBL
        1 | 2, // high_definition_textures
    };
    return config;
}
const PySTKGraphicsConfig & PySTKGraphicsConfig::sd() {
    static PySTKGraphicsConfig config = {600,400,
        false, false, false, false, false,
        2,     // particle_effects
        true,  // animated_characters
        false,  // motionblur
        true,  // mlaa
        true,  // texture_compression
        true,  // ssao
        false, // degraded_IBL
        1 | 2, // high_definition_textures
    };
    return config;
}
const PySTKGraphicsConfig & PySTKGraphicsConfig::ld() {
    static PySTKGraphicsConfig config = {600,400,
        false, false, false, false, false,
        0,     // particle_effects
        false, // animated_characters
        false, // motionblur
        false, // mlaa
        false, // texture_compression
        false, // ssao
        false, // degraded_IBL
        0,     // high_definition_textures
    };
    return config;
}

class PySTKRenderTarget {
    friend class PySTKRace;

private:
    const int BUF_SIZE = 2;
    std::unique_ptr<RenderTarget> rt_;
    std::vector<std::shared_ptr<NumpyPBO> > color_buf_, depth_buf_, instance_buf_;
    int buf_num_=0;

protected:
    void render(irr::scene::ICameraSceneNode* camera, float dt);
    void fetch(std::shared_ptr<PySTKRenderData> data);
    
public:
    PySTKRenderTarget(std::unique_ptr<RenderTarget>&& rt);
    
};

Kart::Kart(int number)
    : m_kart(World::getWorld()->getPlayerKart(number))
{
}

void Kart::getSurroundings()
{
	int radius = 5;
	// means that there is a ray for every 10 degrees
	// This is a sphere, so we're shooting on polar coordinates likely
	three_array array;
    const Vec3 cart_xyz = m_kart->getXYZ();

	for (int theta=0; theta <= 360; theta += 10) {
		for (int omega=0; omega <= 360; omega+= 10) {
			float x = radius * cos(omega) * sin(theta);
			float y = radius * sin(omega) * sin(theta);
			float z = radius * cos(theta);

			Vec3 my_vec;

			my_vec[0] = x + cart_xyz[0];
			my_vec[1] = x + cart_xyz[1];
			my_vec[2] = x + cart_xyz[2];

			btCollisionWorld::ClosestRayResultCallback ray_callback(cart_xyz,
									       	my_vec);
            Physics::get()->getPhysicsWorld()->rayTest(cart_xyz, my_vec, ray_callback);

            // Physics::get()->getPhysicsWorld()->rayTest(cart_xyz, my_vec, ray_callback);

			if(ray_callback.hasHit()) {
				float distance = abs(sqrt(pow(cart_xyz[0] - my_vec[0], 2) +
							  pow(cart_xyz[1] - my_vec[1], 2) +
							  pow(cart_xyz[2] - my_vec[2], 2)));

				for (int i=0; i<radius; i++) {
					if (distance < i) {
						array[theta][omega][i] = 0;
					}
					else {
						btCollisionObject* hit_object = ray_callback.m_collisionObject;
						const UserPointer *upA = (UserPointer*)(hit_object->getUserPointer());

						// 1) object A is a track
						// =======================
						if(upA->is(UserPointer::UP_TRACK))
						{
							array[theta][omega][i] = 1;
						}
						// 2) object a is a kart
						// =====================
						else if(upA->is(UserPointer::UP_KART))
						{
							array[theta][omega][i] = 2;
						}
						// 3) object is a projectile
						// =========================
						else if(upA->is(UserPointer::UP_FLYABLE))
						{
							array[theta][omega][i] = 3;
						}
						// Object is a physical object
						// ===========================
						else if(upA->is(UserPointer::UP_PHYSICAL_OBJECT))
						{
							array[theta][omega][i] = 4;
						}
						else if (upA->is(UserPointer::UP_ANIMATION))
						{
							array[theta][omega][i] = 5;
						}
						else
						    assert("Unknown user pointer");           // 4) Should never happen
					}
				}

			}
			else {
				for (int i=0; i<radius; i++) {
					array[theta][omega][i] = 0;
				}
			}
		} // for omega
	} // for theta
}

PySTKRenderTarget::PySTKRenderTarget(std::unique_ptr<RenderTarget>&& rt):rt_(std::move(rt)) {
    int W = rt_->getTextureSize().Width, H = rt_->getTextureSize().Height;
    buf_num_ = 0;
    for(int i=0; i<BUF_SIZE; i++) {
        color_buf_.push_back(std::make_shared<NumpyPBO>(W, H, GL_RGB, GL_UNSIGNED_BYTE));
        depth_buf_.push_back(std::make_shared<NumpyPBO>(W, H, GL_DEPTH_COMPONENT, GL_FLOAT));
        instance_buf_.push_back(std::make_shared<NumpyPBO>(W, H, GL_RED_INTEGER, GL_UNSIGNED_INT));
    }
}
void PySTKRenderTarget::render(irr::scene::ICameraSceneNode* camera, float dt) {
    rt_->renderToTexture(camera, dt);
}
void PySTKRenderTarget::fetch(std::shared_ptr<PySTKRenderData> data) {
    RTT * rtts = rt_->getRTTs();
    if (rtts && data) {
        unsigned int W = rtts->getWidth(), H = rtts->getHeight();
        // Read the color and depth image
        data->color_buf_ = color_buf_[buf_num_];
        data->depth_buf_ = depth_buf_[buf_num_];
        data->instance_buf_ = instance_buf_[buf_num_];
        
        data->depth_buf_->read(rtts->getDepthStencilTexture());
        data->color_buf_->read(rtts->getRenderTarget(RTT_COLOR));
        data->instance_buf_->read(rtts->getRenderTarget(RTT_LABEL));
        buf_num_ = (buf_num_+1) % BUF_SIZE;
    }
    
}


void PySTKAction::set(KartControl * control) const {
    control->setAccel(acceleration);
    control->setBrake(brake);
    control->setFire(fire);
    control->setNitro(nitro);
    control->setRescue(rescue);
    control->setSteer(steering_angle);
    control->setSkidControl(drift ? (steering_angle > 0 ? KartControl::SC_RIGHT : KartControl::SC_LEFT) : KartControl::SC_NONE);
}
void PySTKAction::get(const KartControl * control) {
    acceleration = control->getAccel();
    brake = control->getBrake();
    fire = control->getFire();
    nitro = control->getNitro();
    rescue = control->getRescue();
    steering_angle = control->getSteer();
    drift = control->getSkidControl() != KartControl::SC_NONE;
}

PySTKRace * PySTKRace::running_kart = 0;
static int is_init = 0;
#ifdef RENDERDOC
static RENDERDOC_API_1_1_2 *rdoc_api = NULL;
#endif
void PySTKRace::init(const PySTKGraphicsConfig & config) {
    if (running_kart)
        throw std::invalid_argument("Cannot init while supertuxkart is running!");
    if (is_init) {
        throw std::invalid_argument("PySTK already initialized! Call clean first!");
    } else {
        is_init = 1;
        initUserConfig();
        stk_config->load(file_manager->getAsset("stk_config.xml"));
        initGraphicsConfig(config);
        initRest();
        load();
    }
#ifdef RENDERDOC

#ifdef _WIN32
    if(HMODULE mod = GetModuleHandleA("renderdoc.dll"))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI =
            (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void **)&rdoc_api);
        assert(ret == 1);
    }
#elif defined(__linux__)
    if(void *mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD))
    {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void **)&rdoc_api);
        assert(ret == 1);
    }
#endif

#endif
}
void PySTKRace::clean() {
    if (running_kart)
        throw std::invalid_argument("Cannot clean up while supertuxkart is running!");
    if (is_init) {
        cleanSuperTuxKart();
        Log::flushBuffers();

        delete file_manager;
        file_manager = NULL;
        is_init = 0;
    }
}
bool PySTKRace::isRunning() { return running_kart; }
PySTKRace::PySTKRace(const PySTKRaceConfig & config) {
    if (running_kart)
        throw std::invalid_argument("Cannot run more than one supertux instance per process!");
    if (!is_init)
        throw std::invalid_argument("PySTK not initialized yet! Call pystk.init().");
    running_kart = this;
    
    resetObjectId();
    
    setupConfig(config);
    for(int i=0; i<config.players.size(); i++)
        render_targets_.push_back( std::make_unique<PySTKRenderTarget>(irr_driver->createRenderTarget( {(unsigned int)UserConfigParams::m_width, (unsigned int)UserConfigParams::m_height}, "player"+std::to_string(i))) );
    
}
std::vector<std::string> PySTKRace::listTracks() {
    if (track_manager)
        return track_manager->getAllTrackIdentifiers();
    return std::vector<std::string>();
}
std::vector<std::string> PySTKRace::listKarts() {
    if (kart_properties_manager)
        return kart_properties_manager->getAllAvailableKarts();
    return std::vector<std::string>();
}
PySTKRace::~PySTKRace() {
    running_kart = nullptr;
}

class LocalPlayerAIController: public Controller {
public:
    Controller * ai_controller_;
public:
    LocalPlayerAIController(Controller * ai_controller):Controller(ai_controller->getKart()), ai_controller_(ai_controller) {}
    ~LocalPlayerAIController() {
        if (ai_controller_) delete ai_controller_;
    }
    virtual void  reset              ()
    { ai_controller_->reset(); }
    virtual void  update             (int ticks)
    { ai_controller_->update(ticks); }
    virtual void  handleZipper       ()
    { ai_controller_->handleZipper(); }
    virtual void  collectedItem      (const ItemState &item,
                                      float previous_energy=0)
    { ai_controller_->collectedItem(item, previous_energy); }
    virtual void  crashed            (const AbstractKart *k)
    { ai_controller_->crashed(k); }
    virtual void  crashed            (const Material *m)
    { ai_controller_->crashed(m); }
    virtual void  setPosition        (int p)
    { ai_controller_->setPosition(p); }
    /** This function checks if this is a local player. A local player will get 
     *  special graphical effects enabled, has a camera, and sound effects will
     *  be played with normal volume. */
    virtual bool  isLocalPlayerController () const { return true; }
    /** This function checks if this player is not an AI, i.e. it is either a
     *  a local or a remote/networked player. This is tested e.g. by the AI for
     *  rubber-banding. */
    virtual bool  isPlayerController () const { return true; }
    virtual bool  disableSlipstreamBonus() const
    { return ai_controller_->disableSlipstreamBonus(); }

    // ------------------------------------------------------------------------
    /** Default: ignore actions. Only PlayerController get them. */
    virtual bool action(PlayerAction action, int value, bool dry_run=false)
    { return ai_controller_->action(action, value, dry_run); }
    // ------------------------------------------------------------------------
    /** Callback whenever a new lap is triggered. Used by the AI
     *  to trigger a recomputation of the way to use.            */
    virtual void  newLap(int lap)
    { return ai_controller_->newLap(lap); }
    // ------------------------------------------------------------------------
    virtual void  skidBonusTriggered()
    { return ai_controller_->skidBonusTriggered(); }
    // ------------------------------------------------------------------------
    /** Called whan this controller's kart finishes the last lap. */
    virtual void  finishedRace(float time)
    { return ai_controller_->finishedRace(time); }
};
void PySTKRace::restart() {
    World::getWorld()->reset(true /* restart */);
    ItemManager::updateRandomSeed(config_.seed);
    powerup_manager->setRandomSeed(config_.seed);
}

void PySTKRace::start() {
    RaceManager::get()->setupPlayerKartInfo();
    RaceManager::get()->startNew();
    time_leftover_ = 0.f;
    
    for(int i=0; i<config_.players.size(); i++) {
        AbstractKart * player_kart = World::getWorld()->getPlayerKart(i);
        if (config_.players[i].controller == PySTKPlayerConfig::AI_CONTROL)
            player_kart->setController(new LocalPlayerAIController(World::getWorld()->loadAIController(player_kart)));
    }
    ItemManager::updateRandomSeed(config_.seed);
    powerup_manager->setRandomSeed(config_.seed);
}
void PySTKRace::stop() {
    render_targets_.clear();
    if (CVS->isGLSL())
    {
        // Reset screen in case the minimap was drawn
        glViewport(0, 0, irr_driver->getActualScreenSize().Width,
            irr_driver->getActualScreenSize().Height);
    }

    if (World::getWorld())
    {
        RaceManager::get()->exitRace();
    }
}
void PySTKRace::render(float dt) {
    World *world = World::getWorld();

    if (world)
    {
        // Render all views
        for(unsigned int i = 0; i < Camera::getNumCameras() && i < render_targets_.size(); i++) {
            Camera::getCamera(i)->activate(false);
            render_targets_[i]->render(Camera::getCamera(i)->getCameraSceneNode(), dt);
        }
        while (render_data_.size() < render_targets_.size()) render_data_.push_back( std::make_shared<PySTKRenderData>() );
        // Fetch all views
        for(unsigned int i = 0; i < render_targets_.size(); i++) {
            render_targets_[i]->fetch(render_data_[i]);
        }
    }
}

bool PySTKRace::step(const std::vector<PySTKAction> & a) {
    for(int i=0; i<a.size(); i++) {
        KartControl & control = World::getWorld()->getPlayerKart(i)->getControls();
        a[i].set(&control);
    }
    return step();
}
bool PySTKRace::step(const PySTKAction & a) {
    KartControl & control = World::getWorld()->getPlayerKart(0)->getControls();
    a.set(&control);
    return step();
}
bool PySTKRace::step() {
    const float dt = config_.step_size;
    if (!World::getWorld()) return false;
    
#ifdef RENDERDOC
    if(rdoc_api) rdoc_api->StartFrameCapture(NULL, NULL);
#endif

    // Update first
    time_leftover_ += dt;
    int ticks = stk_config->time2Ticks(time_leftover_);
    time_leftover_ -= stk_config->ticks2Time(ticks);
    for(int i=0; i<ticks; i++) {
        World::getWorld()->updateWorld(1);
        World::getWorld()->updateTime(1);
    }
    last_action_.resize(config_.players.size());
    for(int i=0; i<last_action_.size(); i++)
        last_action_[i].get(&World::getWorld()->getPlayerKart(i)->getControls());
    
    PropertyAnimator::get()->update(dt);
    
    // Then render
    if (config_.render) {
        World::getWorld()->updateGraphics(dt);

        irr_driver->minimalUpdate(dt);
        render(dt);
    } else {
        World::getWorld()->updateGraphicsMinimal(dt);
    }

    if (config_.render && !irr_driver->getDevice()->run())
        return false;
#ifdef RENDERDOC
    if(rdoc_api) rdoc_api->EndFrameCapture(NULL, NULL);
#endif
    return RaceManager::get()->getFinishedPlayers() < RaceManager::get()->getNumPlayers();
}

void PySTKRace::load() {
    
    material_manager->loadMaterial();
    // Preload the explosion effects (explode.png)
    ParticleKindManager::get()->getParticles("explosion.xml");

    // Reading the rest of the player data needs the unlock manager to
    // initialise the game slots of all players and the AchievementsManager
    // to initialise the AchievementsStatus, so it is done only now.
    ProjectileManager::get()->loadData();

    // Both item_manager and powerup_manager load models and therefore
    // textures from the model directory. To avoid reading the
    // materials.xml twice, we do this here once for both:
    file_manager->pushTextureSearchPath(file_manager->getAsset(FileManager::MODEL,""), "models");
    const std::string materials_file = file_manager->getAsset(FileManager::MODEL,"materials.xml");
    if(materials_file!="")
    {
        // Some of the materials might be needed later, so just add
        // them all permanently (i.e. as shared). Adding them temporary
        // will actually not be possible: powerup_manager adds some
        // permanent icon materials, which would (with the current
        // implementation) make the temporary materials permanent anyway.
        material_manager->addSharedMaterial(materials_file);
    }
    Referee::init();
    powerup_manager->loadPowerupsModels();
    ItemManager::loadDefaultItemMeshes();
    attachment_manager->loadModels();
    file_manager->popTextureSearchPath();
}

static RaceManager::MinorRaceModeType translate_mode(PySTKRaceConfig::RaceMode mode) {
    switch (mode) {
        case PySTKRaceConfig::NORMAL_RACE: return RaceManager::MINOR_MODE_NORMAL_RACE;
        case PySTKRaceConfig::TIME_TRIAL: return RaceManager::MINOR_MODE_TIME_TRIAL;
        case PySTKRaceConfig::FOLLOW_LEADER: return RaceManager::MINOR_MODE_FOLLOW_LEADER;
        case PySTKRaceConfig::THREE_STRIKES: return RaceManager::MINOR_MODE_3_STRIKES;
        case PySTKRaceConfig::FREE_FOR_ALL: return RaceManager::MINOR_MODE_FREE_FOR_ALL;
        case PySTKRaceConfig::CAPTURE_THE_FLAG: return RaceManager::MINOR_MODE_CAPTURE_THE_FLAG;
        case PySTKRaceConfig::SOCCER: return RaceManager::MINOR_MODE_SOCCER;
    }
    return RaceManager::MINOR_MODE_NORMAL_RACE;
}

void PySTKRace::setupConfig(const PySTKRaceConfig & config) {
    config_ = config;
    RaceManager::get()->setDifficulty(RaceManager::Difficulty(config.difficulty));
    RaceManager::get()->setMinorMode(translate_mode(config.mode));
    RaceManager::get()->setNumPlayers(config.players.size());
    for(int i=0; i<config.players.size(); i++) {
        std::string kart = config.players[i].kart.size() ? config.players[i].kart : (std::string)UserConfigParams::m_default_kart;
        const KartProperties *prop = kart_properties_manager->getKart(kart);
        if (!prop)
            kart = UserConfigParams::m_default_kart;
        RaceManager::get()->setPlayerKart(i, kart);
        RaceManager::get()->setKartTeam(i, (KartTeam)config.players[i].team);
    }
    RaceManager::get()->setReverseTrack(config.reverse);
    if (config.track.length())
        RaceManager::get()->setTrack(config.track);
    else
        RaceManager::get()->setTrack("lighthouse");
    
    RaceManager::get()->setNumLaps(config.laps);
    RaceManager::get()->setNumKarts(config.num_kart);
    RaceManager::get()->setMaxGoal(1<<30);
}

void PySTKRace::initGraphicsConfig(const PySTKGraphicsConfig & config) {
    UserConfigParams::m_width  = config.screen_width;
    UserConfigParams::m_height = config.screen_height;
    UserConfigParams::m_glow = config.glow;
    UserConfigParams::m_bloom = config.bloom;
    UserConfigParams::m_light_shaft = config.light_shaft;
    UserConfigParams::m_dynamic_lights = config.dynamic_lights;
    UserConfigParams::m_dof = config.dof;
    UserConfigParams::m_particles_effects = config.particles_effects;
    UserConfigParams::m_animated_characters = config.animated_characters;
    UserConfigParams::m_motionblur = config.motionblur;
    UserConfigParams::m_mlaa = config.mlaa;
    UserConfigParams::m_texture_compression=  config.texture_compression;
    UserConfigParams::m_ssao = config.ssao;
    UserConfigParams::m_degraded_IBL = config.degraded_IBL;
    UserConfigParams::m_high_definition_textures = config.high_definition_textures;
}


//=============================================================================
/** Initialises the minimum number of managers to get access to user_config.
 */
void PySTKRace::initUserConfig()
{
    file_manager = new FileManager();
    // Some parts of the file manager needs user config (paths for models
    // depend on artist debug flag). So init the rest of the file manager
    // after reading the user config file.
    file_manager->init();

    stk_config              = new STKConfig();      // in case of --stk-config
                                                    // command line parameters
}   // initUserConfig

//=============================================================================
void PySTKRace::initRest()
{
    SP::setMaxTextureSize();
    irr_driver = new IrrDriver();

    if (irr_driver->getDevice() == NULL)
    {
        Log::fatal("main", "Couldn't initialise irrlicht device. Quitting.\n");
    }

    StkTime::init();   // grabs the timer object from the irrlicht device

    // Now create the actual non-null device in the irrlicht driver
    irr_driver->initDevice();

    font_manager = new FontManager();
    font_manager->loadFonts();
    SP::loadShaders();

    // The order here can be important, e.g. KartPropertiesManager needs
    // defaultKartProperties, which are defined in stk_config.
    material_manager        = new MaterialManager      ();
    track_manager           = new TrackManager         ();
    kart_properties_manager = new KartPropertiesManager();
    ProjectileManager::create();
    powerup_manager         = new PowerupManager       ();
    attachment_manager      = new AttachmentManager    ();

    // The maximum texture size can not be set earlier, since
    // e.g. the background image needs to be loaded in high res.
    irr_driver->setMaxTextureSize();
    KartPropertiesManager::addKartSearchDir(
                 file_manager->getAddonsFile("karts/"));
    track_manager->addTrackSearchDir(
                 file_manager->getAddonsFile("tracks/"));

    {
        XMLNode characteristicsNode(file_manager->getAsset("kart_characteristics.xml"));
        kart_properties_manager->loadCharacteristics(&characteristicsNode);
    }

    track_manager->loadTrackList();

    RaceManager::create();
    // default settings for Quickstart
    RaceManager::get()->setNumPlayers(1);
    RaceManager::get()->setNumLaps   (3);
    RaceManager::get()->setMinorMode (RaceManager::MINOR_MODE_NORMAL_RACE);
    RaceManager::get()->setDifficulty(
                 (RaceManager::Difficulty)(int)UserConfigParams::m_difficulty);

    kart_properties_manager -> loadAllKarts(false);

}   // initRest

//=============================================================================
/** Frees all manager and their associated memory.
 */
void PySTKRace::cleanSuperTuxKart()
{
    // Stop music (this request will go into the sfx manager queue, so it needs
    // to be done before stopping the thread).
    irr_driver->updateConfigIfRelevant();
    RaceManager::destroy();
    if(attachment_manager)      delete attachment_manager;
    attachment_manager = nullptr;
    ItemManager::removeTextures();
    if(powerup_manager)         delete powerup_manager;
    powerup_manager = nullptr;
    ProjectileManager::destroy();
    if(kart_properties_manager) delete kart_properties_manager;
    kart_properties_manager = nullptr;
    if(track_manager)           delete track_manager;
    track_manager = nullptr;
    if(material_manager)        delete material_manager;
    material_manager = nullptr;
    
    Referee::cleanup();
    ParticleKindManager::destroy();
    if(font_manager)            delete font_manager;
    font_manager = nullptr;
    
    StkTime::destroy();

    // Now finish shutting down objects which a separate thread. The
    // RequestManager has been signaled to shut down as early as possible,
    // the NewsManager thread should have finished quite early on anyway.
    // But still give them some additional time to finish. It avoids a
    // race condition where a thread might access the file manager after it
    // was deleted (in cleanUserConfig below), but before STK finishes and
    // the OS takes all threads down.

    cleanUserConfig();
}   // cleanSuperTuxKart

//=============================================================================
/**
 * Frees all the memory of initUserConfig()
 */
void PySTKRace::cleanUserConfig()
{
    if(stk_config)              delete stk_config;
    stk_config = nullptr;

    if(irr_driver)              delete irr_driver;
    irr_driver = nullptr;
}   // cleanUserConfig
