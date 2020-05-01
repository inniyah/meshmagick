/*
   This file is part of MeshMagick - An Ogre mesh file manipulation tool.
   Copyright (C) 2007-2009 Daniel Wickert

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
   */

#include "MmMeshMergeTool.h"

#include <stdexcept>
#include <OgreAnimation.h>
#include <OgreAxisAlignedBox.h>
#include <OgreHardwareBufferManager.h>
#include <OgreMeshManager.h>
#include <OgreSkeletonManager.h>
#include <OgreSubMesh.h>

#include "MmOgreEnvironment.h"

using namespace Ogre;

//New shared ptr API introduced in 1.10.1
#if OGRE_VERSION >= 0x10A01
#define OGRE_RESET(_sharedPtr) ((_sharedPtr).reset())
#define OGRE_ISNULL(_sharedPtr) (!(_sharedPtr))
#define OGRE_STATIC_CAST(_resourcePtr, _castTo) (Ogre::static_pointer_cast<_castTo>(_resourcePtr))
#else
#define OGRE_RESET(_sharedPtr) ((_sharedPtr).setNull())
#define OGRE_ISNULL(_sharedPtr) ((_sharedPtr).isNull())
#define OGRE_STATIC_CAST(_resourcePtr, _castTo) ((_resourcePtr).staticCast<Ogre::Material>(_castTo))
#endif

namespace meshmagick
{
	MeshMergeTool::MeshMergeTool()
		: mBaseSkeleton(), mMeshes()
	{
	}

	MeshMergeTool::~MeshMergeTool()
	{
		mMeshes.clear();
		OGRE_RESET(mBaseSkeleton);
	}

    Ogre::String MeshMergeTool::getName() const
    {
        return "meshmerge";
    }

	void MeshMergeTool::doInvoke(const OptionList& toolOptions,
			const Ogre::StringVector& inFileNames,
			const Ogre::StringVector& outFileNames)
	{
		if (outFileNames.size() != 1)
		{
			fail("Exactly one output file must be specified.");
			return;
		}
		else if (inFileNames.size() == 0)
		{
			fail("No input files specified.");
			return;
		}

		StatefulMeshSerializer* meshSer = OgreEnvironment::getSingleton().getMeshSerializer();
		StatefulSkeletonSerializer* skelSer =
			OgreEnvironment::getSingleton().getSkeletonSerializer();
		for (Ogre::StringVector::const_iterator it = inFileNames.begin();
			it != inFileNames.end(); ++it)
		{
			MeshPtr curMesh = meshSer->loadMesh(*it);
			if (!OGRE_ISNULL(curMesh))
			{
				if (curMesh->hasSkeleton() &&
					OGRE_ISNULL(SkeletonManager::getSingleton().getByName(curMesh->getSkeletonName())))
				{
					skelSer->loadSkeleton(curMesh->getSkeletonName());
				}
				addMesh(curMesh);
			}
			else
			{
				warn("Skipped: Mesh " + *it + " cannnot be loaded.");
			}
		}
		Ogre::String outputfile = *outFileNames.begin();
		meshSer->exportMesh(merge(outputfile).getPointer(), outputfile);
	}


	void MeshMergeTool::addMesh(Ogre::MeshPtr mesh)
	{
		SkeletonPtr meshSkel = mesh->getSkeleton();
		if (OGRE_ISNULL(meshSkel) && mesh->hasSkeleton())
		{
			meshSkel = SkeletonManager::getSingleton().getByName(mesh->getSkeletonName());
		}

		if (OGRE_ISNULL(meshSkel) && !OGRE_ISNULL(mBaseSkeleton))
		{
			throw std::logic_error(
					"Some meshes have a skeleton, but others have none, cannot merge.");
		}

		if (!OGRE_ISNULL(meshSkel) && OGRE_ISNULL(mBaseSkeleton) && !mMeshes.empty())
		{
			throw std::logic_error(
					"Some meshes have a skeleton, but others have none, cannot merge.");
		}

		if (!OGRE_ISNULL(meshSkel) && OGRE_ISNULL(mBaseSkeleton) && mMeshes.empty())
		{
			mBaseSkeleton = meshSkel;
			print("Set: base skeleton (" + mBaseSkeleton->getName()+")", V_HIGH);
		}

		if (meshSkel != mBaseSkeleton)
		{
			throw std::logic_error(
					"Some meshes have a skeleton, but others have none, cannot merge.");
		}

		mMeshes.push_back(mesh);
	}

	const String MeshMergeTool::findSubmeshName(MeshPtr m, Ogre::ushort sid) const
	{
		Mesh::SubMeshNameMap map = m->getSubMeshNameMap();
		for (Mesh::SubMeshNameMap::const_iterator it = map.begin();
				it != map.end(); ++it)
		{
			if (it->second == sid)
				return it->first;
		}

		return "";
	}

	MeshPtr MeshMergeTool::merge(const Ogre::String& name, const Ogre::String& resourceGroupName)
	{
		print("Baking: New Mesh started", V_HIGH);

		MeshPtr mp = MeshManager::getSingleton().createManual(name, resourceGroupName);

		if (!OGRE_ISNULL(mBaseSkeleton))
		{
			mp->setSkeletonName(mBaseSkeleton->getName());
		}

		AxisAlignedBox totalBounds = AxisAlignedBox();
		for (std::vector<Ogre::MeshPtr>::iterator it = mMeshes.begin(); it != mMeshes.end(); ++it)
		{
			print("Baking: adding submeshes for " + (*it)->getName(), V_HIGH);

			// insert all submeshes
			for (Ogre::ushort sid = 0; sid < (*it)->getNumSubMeshes(); ++sid)
			{
				SubMesh* sub = (*it)->getSubMesh(sid);
				const String name = findSubmeshName((*it), sid);

				// create submesh with correct name
				SubMesh* newsub;
				if (name.length() == 0)
				{
					newsub = mp->createSubMesh();
				}
				else
				{
					/// @todo check if a submesh with this name has been created before
					newsub = mp->createSubMesh(name);
				}

				newsub->useSharedVertices = sub->useSharedVertices;

				// add index
				newsub->indexData = sub->indexData->clone();

				// add geometry
				if (!newsub->useSharedVertices)
				{
					newsub->vertexData = sub->vertexData->clone();

					if (!OGRE_ISNULL(mBaseSkeleton))
					{
						// build bone assignments
						SubMesh::BoneAssignmentIterator bit = sub->getBoneAssignmentIterator();
						while (bit.hasMoreElements())
						{
							VertexBoneAssignment vba = bit.getNext();
							newsub->addBoneAssignment(vba);
						}
					}
				}

				newsub->setMaterialName(sub->getMaterialName());

				// Add vertex animations for this submesh
				Animation *anim = 0;
				for (unsigned short i = 0; i < (*it)->getNumAnimations(); ++i)
				{
					anim = (*it)->getAnimation(i);

					// get or create the animation for the new mesh
					Animation *newanim;
					if (mp->hasAnimation(anim->getName()))
					{
						newanim = mp->getAnimation(anim->getName());
					}
					else
					{
						newanim = mp->createAnimation(anim->getName(), anim->getLength());
					}

					print("Baking: adding vertex animation "
						+ anim->getName() + " for " + (*it)->getName(), V_HIGH);

					Animation::VertexTrackIterator vti=anim->getVertexTrackIterator();
					while (vti.hasMoreElements())
					{
						VertexAnimationTrack *vt = vti.getNext();

						// handle=0 targets the main mesh, handle i (where i>0) targets submesh i-1.
						// In this case there are only submeshes so index 0 will not be used.
						unsigned short handle = mp->getNumSubMeshes();
						VertexAnimationTrack* newvt = newanim->createVertexTrack(
								handle,
								vt->getAssociatedVertexData()->clone(),
								vt->getAnimationType());
						for (int keyFrameIndex = 0; keyFrameIndex < vt->getNumKeyFrames();
							++keyFrameIndex)
						{
							switch (vt->getAnimationType())
							{
								case VAT_MORPH:
								{
									// copy the keyframe vertex buffer
									VertexMorphKeyFrame *kf =
										vt->getVertexMorphKeyFrame(keyFrameIndex);
									VertexMorphKeyFrame *newkf =
										newvt->createVertexMorphKeyFrame(kf->getTime());
									// This creates a ref to the buffer in the original model
									// so don't delete it until the export is completed.
									newkf->setVertexBuffer(kf->getVertexBuffer());
									break;
								}
								case VAT_POSE:
								{
									/// @todo implement pose amination merge
									break;
								}
								case VAT_NONE:
								default:
								{
									break;
								}
							}
						}
					}
				}

				print("Baking: adding submesh '" +
					name + "'  with material " + sub->getMaterialName(), V_HIGH);
			}

			// sharedvertices
			if ((*it)->sharedVertexData)
			{
				/// @todo merge with existing sharedVertexData
				if (!mp->sharedVertexData)
				{
					mp->sharedVertexData = (*it)->sharedVertexData->clone();
				}

				if (!OGRE_ISNULL(mBaseSkeleton))
				{
					Mesh::BoneAssignmentIterator bit = (*it)->getBoneAssignmentIterator();
					while (bit.hasMoreElements())
					{
						VertexBoneAssignment vba = bit.getNext();
						mp->addBoneAssignment(vba);
					}
				}
			}

			print("Baking: adding bounds for " + (*it)->getName(), V_HIGH);

			// add bounds
			totalBounds.merge((*it)->getBounds());
		}
		mp->_setBounds(totalBounds);

		/// @todo merge submeshes with same material

		/// @todo add parameters
		mp->buildEdgeList();

		print("Baking: Finished", V_HIGH);

		reset();

		return mp;
	}

	void MeshMergeTool::reset()
	{
		mMeshes.clear();
	}
}
